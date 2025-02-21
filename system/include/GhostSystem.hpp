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

    bool Set(const std::string &key, const std::string &value);
    std::optional<std::string> Get(const std::string &key);
    void AcknowledgeSync(const std::string &key);
  };

  using props_accessor =
      tbb::concurrent_hash_map<std::string, Properties>::accessor;
  using next_update_accessor =
      tbb::concurrent_hash_map<std::string, nlohmann::json>::accessor;

  static GhostSystem &GetInstance();

  ~GhostSystem();

  void UpdatePropertyState(const std::string &eid, const std::string &key,
                           const std::string &value);
  void ApplyUpdate(const std::string &jsonUpdate);

  std::optional<std::string> PollPropertyUpdate(const std::string &eid,
                                                const std::string &key);

#ifdef CELTE_SERVER_MODE_ENABLED
  void StartReplicationUploadWorker();
  void FetchNextUpdate(nlohmann::json &update);
  void ProcessUpdate(nlohmann::json &update);
  void ReplicateData(const std::string &containerId,
                     const nlohmann::json &entities);
#endif

  static void TryRemoveEntity(const std::string &eid);
  static void HandleReplicationPacket(const req::ReplicationDataPacket &data);

private:
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