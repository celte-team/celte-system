#include "AuthorityTransfer.hpp"
#include "Clock.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"
#include "GrapeRegistry.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include "UniqueProcedures.hpp"
#include <nlohmann/json.hpp>

using namespace celte;

#ifdef CELTE_SERVER_MODE_ENABLED
static void __notifyTakeAuthority(nlohmann::json args) {
  LOGINFO("AuthorityTransfer: Notifying container to take authority.\n" +
          args.dump());
  CallContainerTakeAuthority()
      .on_scope(args["t"].get<std::string>())
      .on_fail_log_error()
      .fire_and_forget(args.dump());
}

static void __notifyDrop(nlohmann::json args) {
  LOGINFO("AuthorityTransfer: Notifying container to drop authority.\n" +
          args.dump());
  CallContainerDropAuthority()
      .on_scope(args["f"].get<std::string>())
      .on_fail_log_error()
      .fire_and_forget(args.dump());
}
#endif

static void prettyPrintAuthTransfer(nlohmann::json args) {
  std::cout << "\n\033[1;32m[AT]:\033[0m entity ";
  std::cout << args["e"].get<std::string>().substr(0, 4) << " ("
            << args["f"].get<std::string>().substr(0, 4) << " -> "
            << args["t"].get<std::string>().substr(0, 4) << ")" << std::endl;
  std::cout << std::endl;
}

void AuthorityTransfer::TransferAuthority(const std::string &entityId,
                                          const std::string &toContainerId,
                                          const std::string &payload,
                                          bool ignoreNoMove) {
  bool abort = true;
  std::string fromContainerId;

  ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
    // if ett is to be assigned to the container that it is already in, we
    // don't need to do anything
    fromContainerId = e.ownerContainerId;
    if (e.quarantine || !e.isValid ||
        (e.ownerContainerId == toContainerId && !ignoreNoMove)) {
      return;
    }
    abort = false;
    e.quarantine = true;
    fromContainerId = e.ownerContainerId;
  });

#ifdef CELTE_SERVER_MODE_ENABLED
  if (abort) {
    return;
  }
  const std::string procedureId = RUNTIME.GenUUID();
  nlohmann::json args;
  args["e"] = entityId;
  args["t"] = toContainerId;
  args["f"] = fromContainerId;
  args["p"] = procedureId;
  args["w"] =
      Clock::ToISOString(2000_ms_later); // change will take effect in 2 seconds
  args["payload"] = payload;
  args["g"] = GHOSTSYSTEM.PeekProperties(entityId).value_or("{}");

  LOGGER.log(celte::Logger::DEBUG, "AuthorityTransfer: \n" + args.dump());
  // #ifdef DEBUG
  prettyPrintAuthTransfer(args);
  // #endif

  // notify the container that will take authority
  __notifyTakeAuthority(args);

  // some peers may not have the toContainerId locally, so we need to notify the
  // fromContainerId as well
  __notifyDrop(args);
#endif
}

/**
 * @brief Executes the drop order process for an entity.
 *
 * This function checks whether the target container exists for the given
 * entity. If the target container does not exist, the entity is marked as
 * invalid and quarantined. When running in server mode, if the source and
 * target containers differ, the entity is removed from its source container.
 * Finally, if the target container is still absent, the function logs a
 * deletion message and schedules the entity for deletion by pushing a task to
 * the engine.
 *
 * @param e The entity to be processed for the drop order.
 * @param toContainerId Identifier of the target container; absence leads to
 * entity quarantine and deletion.
 * @param fromContainerId Identifier of the source container; if differing from
 * the target, triggers removal of the entity.
 * @param procedureId Identifier for the drop order procedure (currently
 * unused).
 */
static void __execDropOrderImpl(Entity &e, const std::string &toContainerId,
                                const std::string &fromContainerId,
                                const std::string &procedureId) {
  // if toContainerId does not exist here, schedule ett for deletion
  if (!GRAPES.ContainerExists(toContainerId)) {
    e.isValid = false;
    e.quarantine = true;
  }
#ifdef CELTE_SERVER_MODE_ENABLED
  if (fromContainerId != toContainerId) {
    ContainerRegistry::GetInstance().RemoveOwnedEntityFromContainer(
        fromContainerId, e.id);
  }
#endif
  // if toContainerId is not registered here, we need to delete the entity.
  if (not GRAPES.ContainerExists(toContainerId)) {
    std::cout << "to container id not registered ("
              << toContainerId.substr(0, 4) << "), deleting entity"
              << std::endl;
    std::cout << "\033[1;31mDELETE\033[0m " << e.id << std::endl;
    std::string id = e.id;
    std::string payload = e.payload;
    RUNTIME.TopExecutor().PushTaskToEngine(
        [id = std::move(id), payload = std::move(payload)]() {
          RUNTIME.Hooks().onDeleteEntity(id, payload);
          ETTREGISTRY.UnregisterEntity(id);
        });
  }
}

static void applyGhostToEntity(const std::string &entityId,
                               nlohmann::json ghost) {
  try {
    GHOSTSYSTEM.ApplyUpdate(entityId, ghost);
  } catch (const std::exception &e) {
    std::cerr << "Error while applying ghost to entity: " << e.what()
              << std::endl;
  }
}

/**
 * @brief Executes an authority take order based on the provided JSON arguments.
 *
 * This function schedules a task to transfer authority for an entity by
 * updating its owner to a target container at a specified time. It extracts
 * order details from the JSON object, including the entity identifier ("e"),
 * target container identifier ("t"), source container identifier ("f"),
 * procedure identifier ("p"), execution time in ISO format ("w"), payload
 * details ("payload"), and ghost data ("g"). Within the scheduled task, the
 * function updates the entity's ownership and clears its quarantine status if
 * the entity exists. If the entity is not registered, it pushes a task to
 * instantiate the entity and apply ghost data.
 *
 * @param args JSON object containing order details:
 *             - "e": Entity identifier.
 *             - "t": Target container identifier.
 *             - "f": Source container identifier.
 *             - "p": Procedure identifier.
 *             - "w": Execution time in ISO format.
 *             - "payload": Payload with additional transfer details.
 *             - "g": Ghost data for updating the entity.
 *
 * @note In server mode, the new entity is also registered to the target
 * container.
 */
void AuthorityTransfer::ExecTakeOrder(nlohmann::json args) {
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Executing take order.\n" + args.dump());
  std::cout << "AuthorityTransfer: Executing take order.\n"
            << args["e"].get<std::string>().substr(0, 4) << std::endl;
  std::string entityId = args["e"].get<std::string>();
  std::string toContainerId = args["t"].get<std::string>();
  std::string fromContainerId = args["f"].get<std::string>();
  std::string procedureId = args["p"].get<std::string>();
  std::string when = args["w"].get<std::string>();
  std::string payload = args["payload"].get<std::string>();
  nlohmann::json ghostData = args["g"];

  Clock::timepoint whenTp = Clock::FromISOString(when);

  // CLOCK.ScheduleAt(whenTp, [=]() {
  //   // if ett exists, transfer auth
  ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
    e.ownerContainerId = toContainerId;
    e.quarantine = false;
  });

  // if ett does not exist, schedule it for creation (container will be
  // emplaced)
  if (!ETTREGISTRY.IsEntityRegistered(entityId)) {
    std::cout << "entity will be instantiated: " << entityId.substr(0, 4)
              << std::endl;
    RUNTIME.TopExecutor().PushTaskToEngine(
        [payload, entityId, toContainerId, ghostData]() {
          std::cout << "call instantiate in exec take order" << std::endl;
          ETTREGISTRY.EngineCallInstantiate(entityId, payload, toContainerId);
          applyGhostToEntity(entityId, ghostData);
        });
  } else {
    std::cout << "entity is already registered: " << entityId.substr(0, 4)
              << std::endl;
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  ContainerRegistry::GetInstance().RegisterNewOwnedEntityToContainer(
      toContainerId, entityId);
#endif
  // });
}

void AuthorityTransfer::ExecDropOrder(nlohmann::json args) {
  if (args.is_array() &&
      !args.empty()) { // sometime args is an array, sometimes not. this is a
                       // workaround, not a fix
    args = nlohmann::json::parse(args[0].get<std::string>());
  }
  std::string entityId = args["e"];
  std::string toContainerId = args["t"];
  std::string fromContainerId = args["f"];
  std::string procedureId = args["p"];
  std::string when = args["w"];
  std::string payload = args["payload"].dump();
  Clock::timepoint whenTp = Clock::FromISOString(when);

  if (fromContainerId == toContainerId) {
    // no need to drop authority if the container is the same
    // this happens once when the entity is created
    return;
  }

  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Executing drop order.\n" + args.dump());
  CLOCK.ScheduleAt(
      whenTp, [entityId, toContainerId, fromContainerId, procedureId]() {
        ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
          __execDropOrderImpl(e, toContainerId, fromContainerId, procedureId);
        });
      });
}

#ifdef CELTE_SERVER_MODE_ENABLED
void AuthorityTransfer::ProxyTakeAuthority(const std::string &grapeId,
                                           const std::string &entityId,
                                           const std::string &fromContainerId,
                                           const std::string &payload) {
  CallGrapeProxyTakeAuthority()
      .on_peer(grapeId)
      .on_fail_log_error()
      .with_timeout(std::chrono::milliseconds(1000))
      .retry(3)
      .fire_and_forget(entityId, fromContainerId, payload);
}

void AuthorityTransfer::__rp_proxyTakeAuthority(
    const std::string &newOwnerGrapeId, const std::string &entityId,
    const std::string &fromContainerId, const std::string &payload) {
  //  systems are not in capacity to decide which container is best suited for
  //  this entity
  // so we forward the job to the engine.
  GRAPES.PushNamedTaskToEngine(newOwnerGrapeId, "proxyTakeAuthority", entityId,
                               fromContainerId, payload);
}

#endif