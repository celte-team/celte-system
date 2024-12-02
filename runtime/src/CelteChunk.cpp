#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include "Requests.hpp"
#include "glm/glm.hpp"

namespace celte {
namespace chunks {
Chunk::Chunk(const ChunkConfig &config)
    : _config(config), _combinedId(config.grapeId + "-" + config.chunkId),
      _boundingBox(config.position, config.size, config.localX, config.localY,
                   config.localZ),
      _rpcs({
          .thisPeerUuid = RUNTIME.GetUUID(),
          .listenOn = {tp::PERSIST_DEFAULT + _combinedId + "." +
                       celte::tp::RPCs},
      }) {}

Chunk::~Chunk() {}

std::string Chunk::Initialize() {
  __registerConsumers();
  __registerRPCs();
  return _combinedId;
}

void Chunk::__registerConsumers() {
  std::cout << "Registering consumers for chunk " << _combinedId << std::endl;
  if (not _config.isLocallyOwned) {
    _createReaderStream<req::ReplicationDataPacket>({
        .thisPeerUuid = RUNTIME.GetUUID(),
        .topics = {_combinedId + "." + celte::tp::REPLICATION},
        .subscriptionName = "",
        .exclusive = false,
        .messageHandlerSync =
            [this](const pulsar::Consumer, req::ReplicationDataPacket req) {
              ENTITIES.HandleReplicationData(req.data, req.active);
            },

    });
  }
#ifdef CELTE_SERVER_MODE_ENABLED
  else { // if locally owned, we are the ones sending the data
    _replicationWS = _createWriterStream<req::ReplicationDataPacket>({
        .topic = _combinedId + "." + celte::tp::REPLICATION,
        .exclusive = true, // only one writer per chunk, the owner of the chunk
    });
  }
#endif
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}

void Chunk::__registerRPCs() {
  // REGISTER_RPC(__rp_scheduleEntityAuthorityTransfer,
  //              celte::rpc::Table::Scope::CHUNK, std::string, std::string,
  //              bool, int);
  _rpcs.Register<bool>(
      "__rp_scheduleEntityAuthorityTransfer",
      std::function([this](std::string entityUUID, std::string newOwnerChunkId,
                           bool take, int tick) {
        try {
          __rp_scheduleEntityAuthorityTransfer(entityUUID, newOwnerChunkId,
                                               take, tick);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_scheduleEntityAuthorityTransfer: "
                    << e.what() << std::endl;

          return false;
        }
      }));
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
    KPOOL.Send((const nl::KPool::SendOptions){
        .topic = _combinedId + "." + celte::tp::REPLICATION,
        .headers = std::move(_nextScheduledReplicationData),
        .value = std::string(),
        .autoCreateTopic = false});
  }

  if (not _nextScheduledActiveReplicationData.empty()) {
    KPOOL.Send((const nl::KPool::SendOptions){
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
  std::cout << "invoking rpc to schedule authority transfer on topic "
            << _combinedId << ".rpc" << std::endl;
  RPC.InvokeChunk(_combinedId, "__rp_scheduleEntityAuthorityTransfer", entityId,
                  _combinedId, true, CLOCK.CurrentTick() + 30);
}
#endif

/* --------------------------------------------------------------------------
 */
/*                                    RPCS */
/* --------------------------------------------------------------------------
 */

void Chunk::__rp_scheduleEntityAuthorityTransfer(std::string entityUUID,
                                                 std::string newOwnerChunkId,
                                                 bool take, int tick) {
  if (take) {
    CLOCK.ScheduleAt(tick, [this, entityUUID, newOwnerChunkId]() {
      try {
#ifdef CELTE_SERVER_MODE_ENABLED
        HOOKS.server.authority.onTake(entityUUID, newOwnerChunkId);
#else
        HOOKS.client.authority.onTake(entityUUID, newOwnerChunkId);
#endif
        auto &newOwnerChunk = GRAPES.GetChunkById(newOwnerChunkId);
        ENTITIES.GetEntity(entityUUID).OnChunkTakeAuthority(newOwnerChunk);
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