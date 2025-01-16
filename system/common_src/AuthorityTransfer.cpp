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

void AuthorityTransfer::TransferAuthority(const std::string &entityId,
                                          const std::string &toContainerId,
                                          const std::string &payload) {
  bool abort = true;
  std::string fromContainerId;
  ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
    if (e.quarantine || !e.isValid || e.ownerContainerId == toContainerId) {
      return;
    }
    abort = false;
    e.quarantine = true;
    fromContainerId = e.ownerContainerId;
  });
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

  // notify the container that will take authority
  __notifyTakeAuthority(args);

  // some peers may not have the toContainerId locally, so we need to notify the
  // fromContainerId as well
  __notifyDrop(args);
}

// macros to unpack the json args for auth transfer (multiple methods use this)
#define UNPACK                                                                 \
  std::string entityId = args["e"];                                            \
  std::string toContainerId = args["t"];                                       \
  std::string fromContainerId = args["f"];                                     \
  std::string procedureId = args["p"];                                         \
  std::string when = args["w"];                                                \
  std::string payload = args["payload"];                                       \
  Clock::timepoint whenTp = Clock::FromISOString(when);

static void __execDropOrderImpl(Entity &e, const std::string &toContainerId,
                                const std::string &fromContainerId,
                                const std::string &procedureId) {
  // if toContainerId does not exist here, schedule ett for deletion
  if (!GRAPES.ContainerExists(toContainerId)) {
    e.isValid = false;
    e.quarantine = true;
    // TODO schedule ett for deletion
  }
}

void AuthorityTransfer::ExecTakeOrder(nlohmann::json args) {
  UNPACK;
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Executing take order.\n" + args.dump());
  CLOCK.ScheduleAt(whenTp, [&]() {
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
      // getting the write context to run in the engine
      auto ownerGrapeId = GRAPES.GetOwnerOfContainer(toContainerId);
      if (not ownerGrapeId.has_value()) {
        throw std::runtime_error(
            "AuthorityTransfer: ExecTakeOrder: Container does not exist.");
      }
      std::cout << "puhing instantiate to engine" << std::endl;
      GRAPES.PushTaskToEngine(ownerGrapeId.value(), [=]() {
        ETTREGISTRY.EngineCallInstantiate(entityId, payload, toContainerId);
      });
    }
  });
}

void AuthorityTransfer::ExecDropOrder(nlohmann::json args) {
  UNPACK;
  LOGGER.log(celte::Logger::DEBUG,
             "AuthorityTransfer: Executing drop order.\n" + args.dump());
  CLOCK.ScheduleAt(
      whenTp, [entityId, toContainerId, fromContainerId, procedureId]() {
        ETTREGISTRY.RunWithLock(entityId, [&](Entity &e) {
          __execDropOrderImpl(e, toContainerId, fromContainerId, procedureId);
        });
      });
}

#undef UNPACK