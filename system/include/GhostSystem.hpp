#pragma once
#include "Clock.hpp"
#include "WriterStreamPool.hpp"
#include "systems_structs.pb.h"
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <tbb/concurrent_hash_map.h>
#include <thread>

namespace celte {
class Runtime; // Forward declaration

class GhostSystem {
public:
  struct PropertyInfo {
    std::string value;
    Clock::timepoint lastSync;
    Clock::timepoint lastChange;
  };

  struct Properties {
    std::unordered_map<std::string, PropertyInfo> properties;

    /// @brief Sets the value of a property, and udpates the time of its last
    /// update to now in the cluster's unified time.
    bool Set(const std::string &key, const std::string &value);

    /// @brief Gets the value of a property if it has been updated since the
    /// last sync.
    std::optional<std::string> Get(const std::string &key);

    /// @brief Acknowledges that the property has been synced. (Next get will
    /// return a value)
    void AcknowledgeSync(const std::string &key);
  };

  using props_accessor =
      tbb::concurrent_hash_map<std::string, Properties>::accessor;
  using next_update_accessor =
      tbb::concurrent_hash_map<std::string, nlohmann::json>::accessor;

  static GhostSystem &GetInstance();

  ~GhostSystem();

  /// @brief  Updates the state of a property of an entity. Use it to push an
  /// update on the network.
  /// @param eid
  /// @param key
  /// @param value
  void UpdatePropertyState(const std::string &eid, const std::string &key,
                           const std::string &value);

  /// @brief Applies an update to the properties of the entities. This
  /// implements an update from the network.
  void ApplyUpdate(const std::string &jsonUpdate);
  void ApplyUpdate(const std::string &eid, nlohmann::json &update);

  /// @brief Polls the update of a property of an entity.
  /// @param eid
  /// @param key
  /// @return std::optional<std::string> The value of the property if it has
  /// changed since the last poll.
  std::optional<std::string> PollPropertyUpdate(const std::string &eid,
                                                const std::string &key);

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Starts the replication upload worker. This worker will periodically
  /// fetch the next update and replicate it to the network.
  void StartReplicationUploadWorker();

  /// @brief Fetches the next update to be replicated. Builds a json object for
  /// each container owned by this peer and fills it with the entities that have
  /// been updated and the new values of their properties.
  /// @param update
  void FetchNextUpdate(nlohmann::json &update);

  /// @brief Replicates an update to the network by sending the data to each
  /// containers, in parallel.
  /// @param update
  void ProcessUpdate(nlohmann::json &update);

  /// @brief Sens the replication data of a single container to the network.
  void ReplicateData(const std::string &containerId,
                     const nlohmann::json &entities);
#endif

  /// @brief Returns all the key - value pairs of the properties of an entity,
  /// without affecting the sync state of the properties.
  /// @param eid
  /// @return
  std::optional<nlohmann::json> PeekProperties(const std::string &eid);

  /// @brief Removes an entity from the map of entities being replicated. Use it
  /// when removing an entity from the system.
  static void TryRemoveEntity(const std::string &eid);

  ///@brief Handles a replication packet from the network. This function is
  /// the callback of containers' replication reader streams.
  static void HandleReplicationPacket(const req::ReplicationDataPacket &data);

private:
  /// @brief Runs Functor f with a lock on the properties of an entity.
  template <typename Functor>
  void __withPropertyLock(const std::string &eid, Functor f) {
    props_accessor acc;
    if (_ettProps.find(acc, eid)) {
      f(acc->second);
    } else {
      _ettProps.insert(acc, eid);
      f(acc->second);
    }
  }

  /// @brief Runs Functor f with a lock on the next update to be replicated.
  template <typename Functor>
  void __withNextUpdateLock(const std::string &containerId, Functor f) {
    next_update_accessor acc;
    if (_nextUpdate.find(acc, containerId)) {
      f(acc->second);
    } else {
      _nextUpdate.insert(acc, containerId);
      f(acc->second);
    }
  }

  tbb::concurrent_hash_map<std::string, Properties> _ettProps;
  tbb::concurrent_hash_map<std::string, nlohmann::json> _nextUpdate;
  std::thread _replicationUploadWorker;
  std::atomic_bool _replicationUploadWorkerRunning = false;
};

#define GHOSTSYSTEM celte::GhostSystem::GetInstance()
} // namespace celte