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
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Notifying container to take authority.\n" +
                 args.dump());
  RUNTIME.GetPeerService().GetRPCService().CallVoid(
      tp::rpc(args["t"]), "__rp_containerTakeAuthority", args.dump());
}

static void __notifyDrop(nlohmann::json args) {
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Notifying container to drop authority.\n" +
                 args.dump());
  RUNTIME.GetPeerService().GetRPCService().CallVoid(
      tp::rpc(args["f"]), "__rp_containerDropAuthority", args.dump());
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
  prettyPrintAuthTransfer(args);

  // notify the container that will take authority
  __notifyTakeAuthority(args);

  // some peers may not have the toContainerId locally, so we need to notify the
  // fromContainerId as well
  __notifyDrop(args);
#endif
}

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
    std::cout << "\033[1;31mDELETE\033[0m " << e.id << std::endl;
    std::string id = e.id;
    std::string payload = e.payload;
    RUNTIME.TopExecutor().PushTaskToEngine(
        [id = std::move(id), payload = std::move(payload)]() {
          RUNTIME.Hooks().onDeleteEntity(id, payload);
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

void AuthorityTransfer::ExecTakeOrder(nlohmann::json args) {
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Executing take order.\n" + args.dump());
  std::string entityId = args["e"].get<std::string>();
  std::string toContainerId = args["t"].get<std::string>();
  std::string fromContainerId = args["f"].get<std::string>();
  std::string procedureId = args["p"].get<std::string>();
  std::string when = args["w"].get<std::string>();
  std::string payload = args["payload"].get<std::string>();
  nlohmann::json ghostData = args["g"];

  Clock::timepoint whenTp = Clock::FromISOString(when);

  CLOCK.ScheduleAt(whenTp, [=]() {
    // if ett exists, transfer auth
    ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
      // ettExists = true;
      e.ownerContainerId = toContainerId;
      e.quarantine = false;
    });

    if (!ETTREGISTRY.IsEntityRegistered(entityId)) {
      RUNTIME.TopExecutor().PushTaskToEngine(
          [payload, entityId, toContainerId, ghostData]() {
            ETTREGISTRY.EngineCallInstantiate(entityId, payload, toContainerId);
            applyGhostToEntity(entityId, ghostData);
          });
    }

#ifdef CELTE_SERVER_MODE_ENABLED
    ContainerRegistry::GetInstance().RegisterNewOwnedEntityToContainer(
        toContainerId, entityId);
#endif
  });
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
  RUNTIME.GetPeerService().GetRPCService().CallVoid(
      tp::peer(grapeId), "__rp_proxyTakeAuthority", entityId, fromContainerId,
      payload);
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