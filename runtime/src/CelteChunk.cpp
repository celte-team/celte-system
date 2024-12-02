#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
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

void Chunk::WaitNetworkInitialized() {
  while (not _rpcs.Ready())
    ;
  for (auto rdr : _readerStreams) {
    while (not rdr->Ready())
      ;
  }
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}

void Chunk::__registerRPCs() {
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

  _rpcs.Register<bool>(
      "__rp_spawnPlayer",
      std::function([this](std::string clientId, float x, float y, float z) {
        try {
          ExecSpawnPlayer(clientId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_spawnPlayer: " << e.what() << std::endl;
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

  // note: std move clears the map in place so we don't need to clear it
  // manually :)
  if (not _nextScheduledReplicationData.empty()) {
    _replicationWS->Write<req::ReplicationDataPacket>(
        req::ReplicationDataPacket{
            .data = std::move(_nextScheduledReplicationData),
            .active = false,
        });
  }

  if (not _nextScheduledActiveReplicationData.empty()) {
    _replicationWS->Write<req::ReplicationDataPacket>(
        req::ReplicationDataPacket{
            .data = std::move(_nextScheduledActiveReplicationData),
            .active = true,
        });
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
  _rpcs.CallVoid(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                 "__rp_scheduleEntityAuthorityTransfer", entityId, _combinedId,
                 true, CLOCK.CurrentTick() + 30);
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
  std::cout << "Scheduling entity authority transfer" << std::endl;
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

void Chunk::SpawnPlayerOnNetwork(const std::string &clientId, float x, float y,
                                 float z) {
  _rpcs.CallVoid(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                 "__rp_spawnPlayer", clientId, x, y, z);
}

void Chunk::ExecSpawnPlayer(const std::string &clientId, float x, float y,
                            float z) {
#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
#else
  HOOKS.client.player.execPlayerSpawn(clientId, x, y, z);
#endif
}

} // namespace chunks
} // namespace celte