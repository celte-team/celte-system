#pragma once
#include "CelteEntity.hpp"
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
   * @brief Returns a reference to the entity with the given uuid.
   *
   * @throws std::out_of_range if the entity is not found.
   */
  celte::CelteEntity &GetEntity(const std::string &uuid) const;

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
   * chunk's repl topic when there is an update but the client needs to be able
   * to receive the data and udpate it locally. This is achieved by registering
   * a consumer dedicated to this task.
   *
   * @note a server is technically a client of other servers when it comes to
   * other server's chunks.
   *
   * @param chunkId The id of the chunk to register the consumer for. Both
   * server nodes and clients can register a consumer for a chunk if there are
   * interested in the updates that are published in the chunk. A server node
   * will tipically not create this consumer for the chunks it manages but will
   * do so for chunks managed by other nodes.
   */
  void RegisterReplConsumer(const std::string &chunkId);

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Returns a summary of all the entities that are currently registered
   * with celte, along with information about how to instantiate them (see
   * CelteEntity::SetInformationToLoad).
   *
   * The data is sent under serialized JSON format.
   * The returned data is under the format:
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
  std::string GetRegisteredEntitiesSummary();
#endif

private:
#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Replicates all entities to their respective chunks.
   */
  void __replicateAllEntities();
#endif

  /**
   * @brief this method is called as a callback when an update is received from
   * a chunk's replication data topic. It will iterate over the entities and
   * update their properties to reflect the values dictated by the server node.
   */
  void __handleReplicationDataReceived(
      std::unordered_map<std::string, std::string> &data);

  std::unordered_map<std::string, std::shared_ptr<CelteEntity>> _entities;

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