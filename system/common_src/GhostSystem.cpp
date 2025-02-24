#include "Clock.hpp"
#include "Container.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include <tbb/parallel_for_each.h>

using namespace celte;

bool GhostSystem::Properties::Set(const std::string &key,
                                  const std::string &value) {
  auto &prop = properties[key]; // created if it doesn't exist
  if (prop.value != value) {
    prop.value = value;
    prop.lastChange = CLOCK.GetUnifiedTime();
    return true;
  }
  return false;
}

std::optional<std::string>
GhostSystem::Properties::Get(const std::string &key) {
  auto it = properties.find(key);
  if (it != properties.end() && it->second.lastSync < it->second.lastChange) {
    return it->second.value;
  }
  return std::nullopt;
}

void GhostSystem::Properties::AcknowledgeSync(const std::string &key) {
  auto it = properties.find(key);
  if (it != properties.end()) {
    it->second.lastSync = it->second.lastChange;
  }
}

GhostSystem &GhostSystem::GetInstance() {
  static GhostSystem instance;
  return instance;
}

GhostSystem::~GhostSystem() {
  _replicationUploadWorkerRunning = false;
  if (_replicationUploadWorker.joinable()) {
    _replicationUploadWorker.join();
  }
}

void GhostSystem::UpdatePropertyState(const std::string &eid,
                                      const std::string &key,
                                      const std::string &value) {
  // If entity is not locally owned, the property won't be changed.
  const std::string &containerId = ETTREGISTRY.GetEntityOwnerContainer(eid);
  if (containerId.empty() or
      not ContainerRegistry::GetInstance().ContainerIsLocallyOwned(
          containerId)) {
    return;
  }
  __withPropertyLock(
      eid, [this, containerId, eid, key, value](Properties &props) {
        if (props.Set(key, value)) {
          // if the value has changed we schedule it for sending in the next
          // update.
          __withNextUpdateLock(containerId,
                               [eid, key, value](nlohmann::json &nextUpdate) {
                                 if (!nextUpdate.contains(eid)) {
                                   nextUpdate[eid] = nlohmann::json::object();
                                 }
                                 nextUpdate[eid][key] = value;
                               });
        }
      });
}

void GhostSystem::ApplyUpdate(const std::string &eid, nlohmann::json &update) {
  __withPropertyLock(eid, [&update](Properties &props) {
    for (auto &[key, value] : update.items()) {
      props.Set(key, value);
    }
  });
}

void GhostSystem::ApplyUpdate(const std::string &jsonUpdate) {
  try {
    nlohmann::json update = nlohmann::json::parse(jsonUpdate);
    for (auto &[eid, properties] : update.items()) {
      __withPropertyLock(eid, [&properties](Properties &props) {
        for (auto &[key, value] : properties.items()) {
          props.Set(key, value);
        }
      });
    }
  } catch (const std::exception &e) {
    std::cerr << "Error while applying ghosts update: " << e.what()
              << std::endl;
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED

void GhostSystem::StartReplicationUploadWorker() {
  int interval = std::atoi(
      RUNTIME.GetConfig().Get("replication_interval").value_or("100").c_str());
  _replicationUploadWorkerRunning = true;
  _replicationUploadWorker = std::thread([this, interval]() {
    while (_replicationUploadWorkerRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(interval));
      nlohmann::json update;
      FetchNextUpdate(update);
      ProcessUpdate(update);
    }
  });
}

void GhostSystem::FetchNextUpdate(nlohmann::json &update) {
  for (auto it = _nextUpdate.begin(); it != _nextUpdate.end(); ++it) {
    next_update_accessor acc;
    if (_nextUpdate.find(acc, it->first)) {
      update[it->first] = std::move(acc->second);
      acc->second.clear();
    }
  }
}

void GhostSystem::ProcessUpdate(nlohmann::json &update) {
  RUNTIME.GetAsyncTaskScheduler().GetArena().execute([this, &update] {
    tbb::parallel_for_each(update.items().begin(), update.items().end(),
                           [this](auto &container) {
                             const std::string &containerId = container.key();
                             const nlohmann::json &entities = container.value();
                             if (entities.is_null() or entities.empty()) {
                               return;
                             }
                             ReplicateData(containerId, entities);
                           });
  });
}

void GhostSystem::ReplicateData(const std::string &containerId,
                                const nlohmann::json &entities) {
  try {
    std::string jsonUpdate = entities.dump();
    ContainerRegistry::GetInstance().RunWithLock(
        containerId, [jsonUpdate](ContainerRegistry::ContainerRefCell &c) {
          auto ws = c.GetContainer().GetReplicationWriterStream();
          if (ws == nullptr) {
            return; // container is not ready
          }
          req::ReplicationDataPacket packet;
          packet.set_data(jsonUpdate);
          ws->Write(packet);
        });
  } catch (const std::exception &e) {
    std::cerr << "Error while replicating data: " << e.what() << std::endl;
  }
}

#endif

void GhostSystem::TryRemoveEntity(const std::string &eid) {
  auto &instance = GetInstance();
  props_accessor acc;
  if (instance._ettProps.find(acc, eid)) {
    instance._ettProps.erase(acc);
  }
}

void GhostSystem::HandleReplicationPacket(
    const req::ReplicationDataPacket &data) {
  GetInstance().ApplyUpdate(data.data());
}

std::optional<std::string>
GhostSystem::PollPropertyUpdate(const std::string &eid,
                                const std::string &key) {
  std::optional<std::string> value;
  __withPropertyLock(eid, [&value, key](Properties &props) {
    value = props.Get(key);
    if (value.has_value()) {
      props.AcknowledgeSync(key);
    }
  });
  return value;
}

std::optional<nlohmann::json>
GhostSystem::PeekProperties(const std::string &eid) {
  std::optional<nlohmann::json> props = std::nullopt;
  __withPropertyLock(eid, [&props](Properties &p) {
    for (auto &[key, prop] : p.properties) {
      if (not props.has_value()) {
        props = nlohmann::json::object();
      }
      props.value()[key] = prop.value;
    }
  });
  return props;
}