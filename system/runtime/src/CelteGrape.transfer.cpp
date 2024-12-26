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

#ifdef CELTE_SERVER_MODE_ENABLED
// executed on the node that drops
void Grape::RemoteTakeEntity(const std::string &entityId) {
  std::cout << "[[CALL REMOTE TAKE ENTITY by grape " << _options.grapeId << "]]"
            << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId,
                  "__rp_remoteTakeEntity", entityId, _options.grapeId);
}

// executed on the node that takes the entity
bool Grape::__rp_remoteTakeEntity(const std::string &entityId,
                                  const std::string &callerId) {
  ENTITIES.GetEntity(entityId).ExecInEngineLoop([this, entityId, callerId]() {
    try {
      std::cout << "[[remote take entity]]" << std::endl;

      // get the best container for the entity, or create it (can't refuse
      // entity)
      auto best = _rg.GetBestContainerForEntity(ENTITIES.GetEntity(entityId));
      if (not best.has_value()) {
        best = ReplicationGraph::ContainerAffinity{
            .container = _rg.AddContainer(),
            .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE};
      }
      RUNTIME.IO().post([this, best, entityId, callerId]() {
        best->container->WaitNetworkInitialized();
        ScheduleAuthorityTransfer(entityId, callerId, best->container->GetId());
      });
      return true;
    } catch (std::out_of_range &e) {
      std::cerr << "Error in __rp_remoteTakeEntity: " << e.what() << std::endl;
      return false;
    }
  });
}

void Grape::ScheduleAuthorityTransfer(const std::string &entityId,
                                      const std::string &prevOwnerGrapeId,
                                      const std::string &newOwnerContainerId) {
  // we schedule the transfer in the
  // future to give time to everyone to
  // get ready
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + prevOwnerGrapeId + "." + tp::RPCs,
                  "__rp_"
                  "scheduleEntityAuthorityTransfer",
                  entityId, newOwnerContainerId, _options.grapeId,
                  CLOCK.CurrentTick() + _options.transferTickDelay);
}
#endif

// executed by everyone listening on the
// original owner's rpc channel (all peers
// listening to it)
void Grape::__rp_scheduleEntityAuthorityTransfer(
    const std::string &entityUUID, const std::string &newOwnerChunkId,
    const std::string &newOwnerGrapeId, int tick) {

  if (not ENTITIES.IsEntityRegistered(entityUUID)) {
    std::cerr << "Entity " << entityUUID
              << " not found, it is not "
                 "instantiated on this peer"
              << std::endl;
    return;
  }

  std::shared_ptr<Grape> newOwnerGrapePtr = GRAPES.GetGrapePtr(newOwnerGrapeId);
  if (newOwnerGrapePtr == nullptr) {
    // grape not replicated on this peer
    // TODO: @ewen destroy entity, it is
    // out of scope
    std::cout << "entity should be "
                 "destroyed here"
              << std::endl;
    return;
  }

  RUNTIME.IO().post([=]() {
    std::optional<std::shared_ptr<IEntityContainer>> newOwnerContainer =
        newOwnerGrapePtr->GetReplicationGraph().GetContainerOpt(
            newOwnerChunkId);
    if (not newOwnerContainer.has_value()) {
      newOwnerContainer =
          newOwnerGrapePtr->GetReplicationGraph().AddContainer(newOwnerChunkId);
    }

    CLOCK.ScheduleAt(tick, [this, entityUUID, newOwnerContainer]() {
      newOwnerContainer.value()->TakeEntityLocally(entityUUID);
    });
  });
}
} // namespace chunks
} // namespace celte