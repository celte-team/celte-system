#include "AuthorityTransfer.hpp"
#include "Clock.hpp"
#include "ETTRegistry.hpp"
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
  if (abort) {
    return;
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  const std::string procedureId = RUNTIME.GenUUID();
  nlohmann::json args;
  args["e"] = entityId;
  args["t"] = toContainerId;
  args["f"] = fromContainerId;
  args["p"] = procedureId;
  args["w"] =
      Clock::ToISOString(2000_ms_later); // change will take effect in 2 seconds
  args["payload"] = payload;

  std::cout << "\nAUTH TRANSFER:\n";
  std::cout << args.dump() << std::endl << std::endl;

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
    std::cout << "toContainerId does not exist, scheduling for deletion"
              << std::endl;
    e.isValid = false;
    e.quarantine = true;
    // TODO schedule ett for deletion
  }
#ifdef CELTE_SERVER_MODE_ENABLED
  if (fromContainerId != toContainerId) {
    ContainerRegistry::GetInstance().RemoveOwnedEntityFromContainer(
        fromContainerId, e.id);
  }
#endif
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

  Clock::timepoint whenTp = Clock::FromISOString(when);

  CLOCK.ScheduleAt(whenTp, [=]() {
    bool ettExists = false;

    // if ett exists, transfer auth
    ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
      ettExists = true;
      e.ownerContainerId = toContainerId;
      e.quarantine = false;
    });

    // if ett does not exist, schedule it for creation (container will be
    // emplaced)
    if (!ettExists) {
      RUNTIME.TopExecutor().PushTaskToEngine(
          [payload, entityId, toContainerId]() {
            ETTREGISTRY.EngineCallInstantiate(entityId, payload, toContainerId);
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