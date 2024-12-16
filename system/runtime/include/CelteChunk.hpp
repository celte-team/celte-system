#pragma once
#include "RPCService.hpp"
#include "Replicator.hpp"
#include "RotatedBoundingBox.hpp"
#include "topics.hpp"
#include <chrono>
#include <glm/vec3.hpp>
#include <map>
#include <string>

namespace celte {
namespace chunks {
struct ChunkConfig {
  const std::string chunkId;
  const std::string grapeId;
  const glm::ivec3 position;
  const glm::vec3 localX;
  const glm::vec3 localY;
  const glm::vec3 localZ;
  const glm::vec3 size;
  const bool isLocallyOwned;
};

/**
 * @brief A chunk is a region of the world which is under a unique server
 * node's control. All entities or the same chunk are replicated together: a
 * chunk is the smallest container of entities that can be moved from one
 * server node to another.
 *
 * Chunks handle entity replication and authority over entities.
 */
class Chunk : public net::CelteService {
public:
  Chunk(const ChunkConfig &config);
  ~Chunk();

  /**
   * @brief Initializes the chunk. Call this method only once.
   * It could be called in the constructor but is not in order to
   * allow copy constructors to be called without reinitializing the network
   *
   * @return std::string the combined id of the chunk
   */
  std::string Initialize();

  /**
   * @brief Returns true if the given position is inside the chunk.
   */
  bool ContainsPosition(float x, float y, float z) const;

  inline const std::string &GetChunkId() const { return _config.chunkId; }

  inline const std::string &GetGrapeId() const { return _config.grapeId; }

  inline const std::string &GetCombinedId() const { return _combinedId; }

  inline bool IsLocallyOwned() const { return _config.isLocallyOwned; }

  /**
   * @brief  Blocking call: waits until all the reader streams of the chunk
   * are ready to receive messages. This includes the rpc channel of the chunk,
   * as well as replication and input topics.
   */
  void WaitNetworkInitialized();

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Called when an entity enters the chunk. (This should be called by
   * the in engine encapsulation of the chunk, on server side, by the server
   * node owning the chunk). This will notify the chunk that an entity has
   * entered it, and trigger the process of transfering authority over to the
   * chunk.
   */
  void OnEnterEntity(const std::string &entityId);

  /**
   * @brief Adds the data of this entity to the list of data to
   * be sent to the chunk's kafka replication topic.
   */
  void ScheduleReplicationDataToSend(const std::string &entityId,
                                     const std::string &blob,
                                     bool active = false);

  /**
   * @brief Sends the data of the entities to the chunk's kafka replication
   * topic if options.replicationIntervalMs has passed since the last time the
   * data was sent.
   *
   */
  void SendReplicationData();

#endif

  float GetDistanceToPosition(float x, float y, float z) const;

  inline ChunkConfig GetConfig() const { return _config; }

  /**
   * @brief Schedules the spawn of the given client at the given position for
   * all peers listening on this topic.
   */
  void SpawnPlayerOnNetwork(const std::string &clientId, float x, float y,
                            float z);

  /**
   * @brief This RPC will be called by clients when they want to spawn their
   * player in the game world.
   *
   * # Hooks:
   * This RPC refers to the following hooks:
   * - celte::api::HooksTable::server::newPlayerConnected::spawnPlayer
   *
   * @param clientId The UUID of the client that connected to the server.
   * @param x The x coordinate where the player should spawn.
   * @param y The y coordinate where the player should spawn.
   * @param z The z coordinate where the player should spawn.
   */
  void ExecSpawnPlayer(const std::string &clientId, float x, float y, float z);

private:
  /**
   * @brief Registers all consumers for the chunk.
   * The consumers listen for events in the chunk's topic and react to them.
   */
  void __registerConsumers();

  /**
   * @brief Registers RPCs for the available actions of the chunk.
   * The RPCs are registered in the global RPC table.
   */
  void __registerRPCs();

  /* --------------------------------------------------------------------------
   */
  /*                                    RPCS */
  /* --------------------------------------------------------------------------
   */

  /**
   * @brief Schedules an entity authority transfer.
   * The entity will be transferred to the new authority at the given global
   * clock tick.
   *
   * @param entityId the id of the entity to transfer
   * @param newOwnerChunkId the id of the chunk that will take authority
   * @param takeAuthority true if the chunk should take authority, false if it
   * should drop it
   * @param atTick the global clock tick at which the transfer should occur
   */
  void __rp_scheduleEntityAuthorityTransfer(std::string entityId,
                                            std::string newOwnerChunkId,
                                            bool takeAuthority, int atTick);

  /* --------------------------------------------------------------------------
   */
  /*                                   Members */
  /* --------------------------------------------------------------------------
   */

  RotatedBoundingBox _boundingBox;

  const ChunkConfig _config;
  const std::string _combinedId;
  net::RPCService _rpcs;

#ifdef CELTE_SERVER_MODE_ENABLED
  std::shared_ptr<net::WriterStream> _replicationWS;
  std::unordered_map<std::string, std::string> _nextScheduledReplicationData;
  std::unordered_map<std::string, std::string>
      _nextScheduledActiveReplicationData;
#endif
};
} // namespace chunks
} // namespace celte