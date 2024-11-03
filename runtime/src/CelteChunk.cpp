#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include "glm/glm.hpp"

namespace celte {
namespace chunks {
Chunk::Chunk(const ChunkConfig &config)
    : _config(config), _combinedId(config.grapeId + "-" + config.chunkId),
      _boundingBox(config.position, config.size, config.localX, config.localY,
                   config.localZ) {}

Chunk::~Chunk() {}

std::string Chunk::Initialize() {
  __registerConsumers();
  __registerRPCs();
  return _combinedId;
}

void Chunk::__registerConsumers() {
  KPOOL.RegisterTopicCallback(_combinedId + "." + celte::tp::RPCs,
                              [this](auto r) { RPC.InvokeLocal(r); });
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}

void Chunk::__registerRPCs() {
  REGISTER_RPC(__rp_scheduleEntityAuthorityTransfer,
               celte::rpc::Table::Scope::CHUNK, std::string, bool, int);
}

#ifdef CELTE_SERVER_MODE_ENABLED
void Chunk::ScheduleReplicationDataToSend(const std::string &entityId,
                                          const std::string &blob,
                                          bool active) {
  if (active) {
    _nextScheduledActiveReplicationData[entityId] = blob;
  } else {
    _nextScheduledReplicationData[entityId] = blob;
  }
}

void Chunk::SendReplicationData() {
  if (not _config.isLocallyOwned)
    return;

  if (not _nextScheduledReplicationData.empty()) {
    // std::move will clear the local map, so no need to clear it
    KPOOL.Send((const nl::KafkaPool::SendOptions){
        .topic = _combinedId + "." + celte::tp::REPLICATION,
        .headers = std::move(_nextScheduledReplicationData),
        .value = std::string(),
        .autoCreateTopic = false});
  }

  if (not _nextScheduledActiveReplicationData.empty()) {
    KPOOL.Send((const nl::KafkaPool::SendOptions){
        .topic = _combinedId + "." + celte::tp::REPLICATION,
        .headers = std::move(_nextScheduledActiveReplicationData),
        .value = std::string("active"),
        .autoCreateTopic = false});
  }
}

void Chunk::OnEnterEntity(const std::string &entityId) {
  try {
    auto &entity = ENTITIES.GetEntity(entityId);
    if (entity.GetOwnerChunk().GetCombinedId() == _combinedId) {
      std::cerr << "transferring to the same chunk" << std::endl;
      return;
    }
  } catch (std::out_of_range &e) {
    logs::Logger::getInstance().err()
        << "Entity not found in OnEnterEntity: " << e.what() << std::endl;
    std::cerr << "Entity not found in OnEnterEntity: " << std::endl;
  }

  // the current method is only called when the entity enters the chunk in the
  // server node, calling the RPC will trigger the behavior of transfering
  // authority over to the chunk in all the peers listening to the chunk's
  // topic.
  RPC.InvokeChunk(_combinedId, "__rp_scheduleEntityAuthorityTransfer", entityId,
                  true, CLOCK.CurrentTick() + 30);
}
#endif

/* -------------------------------------------------------------------------- */
/*                                    RPCS                                    */
/* -------------------------------------------------------------------------- */

void Chunk::__rp_scheduleEntityAuthorityTransfer(std::string entityUUID,
                                                 bool take, int tick) {
  if (take) {
    CLOCK.ScheduleAt(tick, [this, entityUUID]() {
      try {
        ENTITIES.GetEntity(entityUUID).OnChunkTakeAuthority(*this);
      } catch (std::out_of_range &e) {
        logs::Logger::getInstance().err()
            << "Entity not found: " << e.what() << std::endl;
      }
    });
  }
  // nothing to do (yet) for the chunk loosing the authority... more will come
  // in the future
}

} // namespace chunks
} // namespace celte