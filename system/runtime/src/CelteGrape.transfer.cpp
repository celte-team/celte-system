#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
namespace chunks {

/* -------------------------------------------------------------------------- */
/*                              External transfer                             */
/* -------------------------------------------------------------------------- */

#ifdef CELTE_SERVER_MODE_ENABLED
// executed on the node that drops
void Grape::RemoteTakeEntity(const std::string &entityId) {
  std::cout << "[[CALL REMOTE TAKE ENTITY by grape " << _options.grapeId << "]]"
            << std::endl;
  std::string currOwnerContainerId =
      ENTITIES.GetEntity(entityId).GetContainerId();
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId,
                  "__rp_remoteTakeEntity", entityId, _options.grapeId,
                  currOwnerContainerId);
}

// executed on the node that takes the entity
bool Grape::__rp_remoteTakeEntity(const std::string &entityId,
                                  const std::string &callerId,
                                  const std::string &prevOwnerContainerId) {
  // TODO err handling here if entity does not exist yet!
  ENTITIES.GetEntity(entityId).ExecInEngineLoop([this, entityId, callerId,
                                                 prevOwnerContainerId]() {
    try {
      std::cout << "[[__remote take entity]]" << std::endl;

      // get the best container for the entity, or create it (can't refuse
      // entity)
      auto best = _rg.GetBestContainerForEntity(ENTITIES.GetEntity(entityId));
      if (not best.has_value()) {
        best = ReplicationGraph::ContainerAffinity{
            .container = _rg.AddContainer(),
            .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE};
      }
      RUNTIME.IO().post(
          [this, best, entityId, callerId, prevOwnerContainerId]() {
            best->container->WaitNetworkInitialized();
            ScheduleAuthorityTransfer(entityId, callerId, prevOwnerContainerId,
                                      best->container->GetId());
          });
    } catch (std::out_of_range &e) {
      std::cerr << "Error in __rp_remoteTakeEntity: " << e.what() << std::endl;
    }
  });
  return true;
}

void Grape::ScheduleAuthorityTransfer(const std::string &entityId,
                                      const std::string &prevOwnerGrapeId,
                                      const std::string &prevOwnerContainerId,
                                      const std::string &newOwnerContainerId) {
  // we schedule the transfer in the
  // future to give time to everyone to
  // get ready

  TransferInfo ti{
      .entityId = entityId,
      .gFrom = prevOwnerGrapeId,
      .cFrom = prevOwnerContainerId,
      .gTo = _options.grapeId,
      .cTo = newOwnerContainerId,
  };

  TransferAuthority(ti);

  // _rpcs->CallVoid(tp::PERSIST_DEFAULT + prevOwnerGrapeId + "." + tp::RPCs,
  //                 "__rp_"
  //                 "scheduleEntityAuthorityTransfer",
  //                 entityId, newOwnerContainerId, _options.grapeId,
  //                 informationToLoad,
  //                 CLOCK.CurrentTick() + _options.transferTickDelay);
}
#endif

// executed by everyone listening on the
// original owner's rpc channel (all peers
// listening to it)
void Grape::__rp_scheduleEntityAuthorityTransfer(
    const std::string &entityUUID, const std::string &newOwnerChunkId,
    const std::string &newOwnerGrapeId, const std::string &informationToLoad,
    int tick) {

  throw std::logic_error("__rp_scheduleEntityAuthorityTransfer is deprecated");

  // if (not ENTITIES.IsEntityRegistered(entityUUID)) {
  //   if (GRAPES.GetGrape(newOwnerGrapeId)
  //           .GetReplicationGraph()
  //           .GetContainerOpt(newOwnerChunkId)
  //           .has_value()) {
  //     // entity not instantiated but the container to which it is transfered
  //     // exists so we must spawn it
  //   }
  //   return;
  // }

  // std::shared_ptr<Grape> newOwnerGrapePtr =
  // GRAPES.GetGrapePtr(newOwnerGrapeId); if (newOwnerGrapePtr == nullptr) {
  //   // grape not replicated on this peer
  //   // TODO: @ewen destroy entity, it is
  //   // out of scope
  //   std::cout << "entity should be "
  //                "destroyed here"
  //             << std::endl;
  //   return;
  // }

  // RUNTIME.IO().post([=]() {
  //   std::optional<std::shared_ptr<IEntityContainer>> newOwnerContainer =
  //       newOwnerGrapePtr->GetReplicationGraph().GetContainerOpt(newOwnerChunkId);
  //   if (not newOwnerContainer.has_value()) {
  //     newOwnerContainer =
  //         newOwnerGrapePtr->GetReplicationGraph().AddContainer(newOwnerChunkId);
  //   }

  //   CLOCK.ScheduleAt(tick, [this, entityUUID, newOwnerContainer]() {
  //     newOwnerContainer.value()->TakeEntityLocally(entityUUID);
  //   });
  // });
}

/* -------------------------------------------------------------------------- */
/*                              new algo                             */
/* -------------------------------------------------------------------------- */

static nlohmann::json transferInfoToJson(const TransferInfo &ti) {
  return {
      {"entityId", ti.entityId}, {"gFrom", ti.gFrom}, {"cFrom", ti.cFrom},
      {"gTo", ti.gTo},           {"cTo", ti.cTo},
  };
}

static TransferInfo transferInfoFromJson(const nlohmann::json &j) {
  return {
      .entityId = j["entityId"].get<std::string>(),
      .gFrom = j["gFrom"].get<std::string>(),
      .cFrom = j["cFrom"].get<std::string>(),
      .gTo = j["gTo"].get<std::string>(),
      .cTo = j["cTo"].get<std::string>(),
  };
}

#ifdef CELTE_SERVER_MODE_ENABLED
// this is can be called either by the current owner or the new owner (entity
// must exist locally)
void Grape::TransferAuthority(const TransferInfo &ti) {
  std::cout << "[[TRANSFER AUTHORITY]]" << std::endl;
  auto &entity = ENTITIES.GetEntity(ti.entityId);
  entity.ExecInEngineLoop([this, &entity, ti]() {
    std::string informationToLoad = entity.GetInformationToLoad();
    std::string props = entity.GetProps();
    std::string jsonTI = transferInfoToJson(ti).dump();

    int tick = CLOCK.CurrentTick() + _options.transferTickDelay;
    __containerTakes(ti.cTo, jsonTI, informationToLoad, props, tick);
    __containerDrops(ti.cFrom, jsonTI,
                     tick * 2); // drop later to be sure it is transferred first
  });
}

void Grape::__containerTakes(const std::string &topic,
                             const std::string &transferInfo,
                             const std::string &informationToLoad,
                             const std::string &props, int tick) {
  std::cout << "[[CALLING CONTAINER TAKES]]" << transferInfo << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + topic + "." + tp::RPCs,
                  "__rp_containerTakes", transferInfo, informationToLoad, props,
                  tick);
}

void Grape::__containerDrops(const std::string &topic,
                             const std::string &transferInfo, int tick) {
  std::cout << "[[CALLING CONTAINER DROPS]]" << transferInfo << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + topic + "." + tp::RPCs,
                  "__rp_containerDrops", transferInfo, tick);
}

#endif

} // namespace chunks
} // namespace celte