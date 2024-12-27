#pragma once
#include "RPCService.hpp"
#include "ReplicationGraph.hpp"
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
  std::string chunkId;
  std::string grapeId;
  unsigned int preferredEntityCount;
  float preferredContainerSize;
  bool isLocallyOwned;
};

/**
 * @brief A chunk is a region of the world which is under a unique server
 * node's control. All entities or the same chunk are replicated together: a
 * chunk is the smallest container of entities that can be moved from one
 * server node to another.
 *
 * Chunks handle entity replication and authority over entities.
 */
class Chunk : public IEntityContainer {
public:
  Chunk(const nlohmann::json &config);
  ~Chunk();

  void Load(const nlohmann::json &features) override;

  /**
   * @brief Initializes the chunk. Call this method only once.
   * It could be called in the constructor but is not in order to
   * allow copy constructors to be called without reinitializing the network
   *
   * @return std::string the combined id of the chunk
   */
  std::string Initialize() override;

  void Remove() override;

#ifdef CELTE_SERVER_MODE_ENABLED
  nlohmann::json GetFeatures() override;
#endif

  inline const std::string &GetChunkId() const { return _config.chunkId; }

  inline const std::string &GetGrapeId() const override {
    return _config.grapeId;
  }

  inline const std::string &GetCombinedId() const { return _combinedId; }

  inline std::string GetId() const override { return _combinedId; }

  inline bool IsLocallyOwned() const override { return _config.isLocallyOwned; }

  /**
   * @brief  Blocking call: waits until all the reader streams of the chunk
   * are ready to receive messages. This includes the rpc channel of the chunk,
   * as well as replication and input topics.
   */
  void WaitNetworkInitialized() override;

  /**
   * @brief Returns a handle to the rpc service of the chunk.
   *
   */
  inline net::RPCService &GetRPCService() { return _rpcs; }

  void TakeEntityLocally(const std::string &entityId) override;

  void LoadExistingEntities() override;

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Called when an entity enters the chunk. (This should be called by
   * the in engine encapsulation of the chunk, on server side, by the server
   * node owning the chunk). This will notify the chunk that an entity has
   * entered it, and trigger the process of transfering authority over to the
   * chunk.
   */
  inline void TakeEntity(const std::string &entityId) override;

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

  // /**
  //  * @brief Schedules the spawn of the given client at the given position for
  //  * all peers listening on this topic.
  //  */
  // void SpawnEntityOnNetwork(const std::string &entity, float x, float y,
  //                           float z) override;

#endif

  inline ChunkConfig GetConfig() const { return _config; }
  nlohmann::json GetConfigJSON() const override;

  // /**
  //  * @brief This RPC will be called by clients when they want to spawn their
  //  * player in the game world.
  //  *
  //  * # Hooks:
  //  * This RPC refers to the following hooks:
  //  * - celte::api::HooksTable::server::newPlayerConnected::spawnPlayer
  //  *
  //  * @param clientId The UUID of the client that connected to the server.
  //  * @param x The x coordinate where the player should spawn.
  //  * @param y The y coordinate where the player should spawn.
  //  * @param z The z coordinate where the player should spawn.
  //  */
  // void ExecSpawnPlayer(const std::string &clientId, float x, float y, float
  // z);

  std::set<std::string> GetOwnedEntities() const;

  /**
   * @brief The position of entities is needed to determine which entities
   * to keep in the container. However this information is not available in the
   * systems as it is only available in the engine. Setting a getter using this
   * method allows the chunk to get the position of entities when needed.
   */
  void
  SetEntityPositionGetter(std::function<glm::vec3(const std::string &)> getter);

private:
  void __attachEntityAsync(const std::string &entityId, int retries);

  /**
   * @brief Transfers the authority over the entity to another chunk of the same
   * grape.
   */
  void __rp_chunkScheduleAuthorityTransfer(const std::string &entityUUID,
                                           const std::string &newOwnerChunkId,
                                           int tick);

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
  void ScheduleEntityAuthorityTransfer(std::string entityId,
                                       std::string newOwnerChunkId, int atTick);

  void __rp_containerTakes(const std::string &transferInfo,
                           const std::string &informationToLoad,
                           const std::string &props, int tick);

  void __rp_containerDrops(const std::string &transferInfo, int tick);

#ifdef CELTE_SERVER_MODE_ENABLED
  void __rememberEntity(const std::string &entityId);
  void __forgetEntity(const std::string &entityId);
  void __refreshCentroid();
  glm::vec3 _centroid;
#endif

  /* --------------------------------------------------------------------------
   */
  /*                                   Members */
  /* --------------------------------------------------------------------------
   */

  ChunkConfig _config;
  const std::string _combinedId;

  std::function<glm::vec3(const std::string &)> _entityPositionGetter = nullptr;

#ifdef CELTE_SERVER_MODE_ENABLED
  std::set<std::string> _ownedEntities;
#endif
};
} // namespace chunks
} // namespace celte