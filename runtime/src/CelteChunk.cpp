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

void Chunk::Initialize() {
  __registerConsumers();
  __registerRPCs();
  if (not _config.isLocallyOwned) {
    ENTITIES.RegisterReplConsumer(_combinedId);
  }
}

void Chunk::__registerConsumers() {
  // A consumer to listen for Chunk scope RPCs and execute them
  KPOOL.Subscribe({
      .topic = _combinedId + "." + celte::tp::RPCs,
      .groupId = "", // no group, all consumers receive the message
      .autoCreateTopic = true,
      .autoPoll = true,
      .callback = [this](auto r) { RUNTIME.RPCTable().InvokeLocal(r); },
  });

  KPOOL.CreateTopicIfNotExists(_combinedId + "." + celte::tp::REPLICATION, 1,
                               1);
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
                                          const std::string &blob) {
  _nextScheduledReplicationData[entityId] = blob;
}

void Chunk::SendReplicationData() {
  if (_nextScheduledReplicationData.empty() or not _config.isLocallyOwned) {
    return;
  }

  // std::move will clear the local map, so no need to clear it manually
  KPOOL.Send((const nl::KafkaPool::SendOptions){
      .topic = _combinedId + "." + celte::tp::REPLICATION,
      .headers = std::move(_nextScheduledReplicationData),
      .value = std::string(),
      .autoCreateTopic = false});
}

void Chunk::OnEnterEntity(const std::string &entityId) {
  try {
    auto &entity = ENTITIES.GetEntity(entityId);
    if (entity.GetOwnerChunk().GetCombinedId() == _combinedId) {
      return;
    }
  } catch (std::out_of_range &e) {
    logs::Logger::getInstance().err()
        << "Entity not found in OnEnterEntity: " << e.what() << std::endl;
    return;
  }

  // the current method is only called when the entity enters the chunk in the
  // server node, calling the RPC will trigger the behavior of transfering
  // authority over to the chunk in all the peers listening to the chunk's
  // topic.
  RPC.InvokeChunk(_combinedId, "__rp_scheduleEntityAuthorityTransfer", entityId,
                  true, CLOCK.CurrentTick() + 10);
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