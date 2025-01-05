#pragma once
#include "CelteEntity.hpp"
#include "PendingSpawnInfo.hpp"
#include <atomic>
#include <condition_variable>
#include <future>
#include <optional>
#include <queue>
#include <set>
#include <unordered_map>

namespace celte {
namespace runtime {
class CelteEntityManagementSystem {

public:
  friend class celte::CelteEntity;
  ~CelteEntityManagementSystem();

  /**
   * @brief Registers an entity with the system, enabling quick access to it
   * until it is unregistered.
   *
   * This method should only be called by the CelteEntity class OnSpawn method.
   */
  void RegisterEntity(std::shared_ptr<celte::CelteEntity> entity);

  /**
   * @brief Unregisters an entity from the system.
   *
   * This method should only be called by the CelteEntity class OnDestroy
   * method.
   */
  void UnregisterEntity(std::shared_ptr<celte::CelteEntity> entity);

  /**
   * @brief Returns true if the entity with the given uuid is registered with
   * the system, false otherwise.
   */
  inline bool IsEntityRegistered(const std::string &uuid) {
    std::scoped_lock lock(_entitiesMutex);
    return _entities.find(uuid) != _entities.end();
  }

  /**
   * @brief Returns a reference to the entity with the given uuid.
   *
   * @throws std::out_of_range if the entity is not found.
   */
  CelteEntity &GetEntity(const std::string &uuid);

  /**
   * @brief Returns a shared_ptr to the entity with the given uuid. If the
   * entity does not exist, the nullptr is returned.
   */
  std::shared_ptr<CelteEntity> GetEntityPtr(const std::string &uuid);

  /**
   * @brief Performs the logic common to all entities once. Call this as often
   * as possible.
   */
  void Tick();

  /**
   * @brief Applies a filter to a list of entity ids, returning only the ones
   * that match the filter. The filter must be a string among the following:
   * - "all": no filter
   * - "not instantiated": only entities that are not instantiated
   * - "not registered": only entities that are not registered
   * - "not owned": only entities that are not owned by the current peer
   * - "instantiated": only entities that are instantiated
   * - "registered": only entities that are registered
   * - "owned": only entities that are owned by the current peer
   *
   */
  std::vector<std::string>
  FilterEntities(const std::vector<std::string> &entityIds,
                 const std::string &filter);

  /**
   * @brief This method will register the consumer meant to handle property
   * replication handling. The server automatically sends the data to the
   * chunk's repl topic when there is an update but the client needs to be
   * able to receive the data and udpate it locally. This is achieved by
   * registering a consumer dedicated to this task.
   *
   * @note a server is technically a client of other servers when it comes to
   * other server's chunks.
   *
   * @param chunkId The id of the chunk to register the consumer for. Both
   * server nodes and clients can register a consumer for a chunk if there are
   * interested in the updates that are published in the chunk. A server node
   * will tipically not create this consumer for the chunks it manages but
   * will do so for chunks managed by other nodes.
   */
  void RegisterReplConsumer(const std::vector<std::string> &chunkId);

  void LoadExistingEntities(const std::string &summary);

  /**
   * @brief Quaranteens an entity, preventing it from being processed
   * by the replication graph of all locally instantiated grapes.
   * This is useful when an entity is being transferred to another grape, to
   * avoid double assignment.
   */
  inline void QuaranteenEntity(const std::string &entityId) {
    std::cout << "[[QUARANTEENED]] " << entityId << std::endl;
    std::lock_guard<std::mutex> lock(_quaranteenedEntitiesMutex);
    _quaranteenedEntities.insert(entityId);
  }

  /**
   * @brief Unquaranteens an entity, allowing it to be processed by the
   * replication graph of all locally instantiated grapes.
   *
   * @note This method is called by CelteEntity::OnChunkTakeAuthority, which is
   * itself called by IEntityContainer::TakeEntityLocally.
   */
  inline void UnquaranteenEntity(const std::string &entityId) {
    std::cout << "[[UN-QUARANTEENED]] " << entityId << std::endl;
    std::lock_guard<std::mutex> lock(_quaranteenedEntitiesMutex);
    _quaranteenedEntities.erase(entityId);
  }

  /**
   * @brief Returns true if the entity is quaranteened, false otherwise.
   */
  inline bool IsQuaranteened(const std::string &entityId) {
    std::lock_guard<std::mutex> lock(_quaranteenedEntitiesMutex);
    return _quaranteenedEntities.find(entityId) != _quaranteenedEntities.end();
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  inline void AddPendingSpawn(const std::string &uuid,
                              const PendingSpawnInfo &info) {
    _pendingSpawns[uuid] = info;
  }

  inline void RemovePendingSpawn(const std::string &uuid) {
    _pendingSpawns.erase(uuid);
  }

  inline PendingSpawnInfo GetPendingSpawn(const std::string &uuid) {
    return _pendingSpawns.at(uuid);
  }

  /**
   * @brief Returns a summary of all the entities that are currently
   * registered with celte, along with information about how to instantiate
   * them (see CelteEntity::SetInformationToLoad).
   *
   * The data is sent under serialized JSON format.
   * The returned data is under the format:
   *
   * @param containerId The id of the container to fetch the entities from. If
   * this is left empty, all entities on the server will be returned.
   *
   * @code JSON
   * [
   *  {
   *    "uuid": "uuid",
   *    "chunk": "chunkCombinedId",
   *    "info": "info"
   * },
   * {
   *    "uuid": "uuid",
   *    "chunk": "chunkCombinedId",
   *    "info": "info"
   * }
   * ]
   * @endcode
   *
   * @note This method is only available in server mode.
   */
  std::string GetRegisteredEntitiesSummary(const std::string &containerId = "");
#endif

  inline void
  HandleReplicationData(std::unordered_map<std::string, std::string> &data,
                        bool active = false) {
    __handleReplicationDataReceived(data, active);
  }

private:
#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Replicates all entities to their respective chunks.
   */
  void __replicateAllEntities();

#endif

  /**
   * @brief this method is called as a callback when an update is received
   * from a chunk's replication data topic. It will iterate over the entities
   * and update their properties to reflect the values dictated by the server
   * node.
   */
  void __handleReplicationDataReceived(
      std::unordered_map<std::string, std::string> &data, bool active = false);

  std::unordered_map<std::string, std::shared_ptr<CelteEntity>> _entities;

#ifdef CELTE_SERVER_MODE_ENABLED
  std::unordered_map<std::string, PendingSpawnInfo> _pendingSpawns;
#endif

  /**
   * @brief Waits for entities with the given uuids to be instantiated and
   * then takes them locally (i.e assigns them to the provided chunk).
   *
   * @param uuid The uuid of the entity to take locally.
   * @param chunkId The id of the chunk to assign the entity to.
   * @param deadline The time point at which the operation should be
   * considered failed if not completed.
   */
  void __takeLocallyDeferred(
      const std::string &uuid, const std::string &chunkId, bool isClient,
      std::chrono::time_point<std::chrono::system_clock> deadline);

  std::mutex _entitiesMutex;

  std::set<std::string> _quaranteenedEntities;
  std::mutex _quaranteenedEntitiesMutex;

  /*
  --------------------------------------------------------------------------
                                     FILTERS
  --------------------------------------------------------------------------
   */

  inline bool __filterAll(const std::string &) const { return true; }
  inline bool __filterInstantiated(const std::string &id) const {
    return _entities.at(id)->IsSpawned();
  }
  inline bool __filterRegistered(const std::string &id) const {
    return _entities.find(id) != _entities.end();
  }
  inline bool __filterOwned(const std::string &id) const {
    return _entities.at(id)->GetOwnerChunk().GetConfig().isLocallyOwned;
  }
  inline bool __filterNotInstantiated(const std::string &id) const {
    return _entities.find(id) == _entities.end();
  }
  inline bool __filterNotRegistered(const std::string &id) const {
    return not __filterRegistered(id);
  }
  inline bool __filterNotOwned(const std::string &id) const {
    return not __filterOwned(id);
  }

  std::unordered_map<std::string, bool (CelteEntityManagementSystem::*)(
                                      const std::string &) const>
      _filters = {
          {"all", &CelteEntityManagementSystem::__filterAll},
          {"instantiated", &CelteEntityManagementSystem::__filterInstantiated},
          {"registered", &CelteEntityManagementSystem::__filterRegistered},
          {"owned", &CelteEntityManagementSystem::__filterOwned},
          {"not instantiated",
           &CelteEntityManagementSystem::__filterNotInstantiated},
          {"not registered",
           &CelteEntityManagementSystem::__filterNotRegistered},
          {"not owned", &CelteEntityManagementSystem::__filterNotOwned},
  };
};
} // namespace runtime
} // namespace celte