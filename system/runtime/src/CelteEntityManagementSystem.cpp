#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
// #include <boost/json.hpp>
#include "base64.hpp"
#include "nlohmann/json.hpp"
#include <string>

namespace celte {
namespace runtime {
CelteEntityManagementSystem::~CelteEntityManagementSystem() {
  logs::Logger::getInstance().info()
      << "Destroying entity management system." << std::endl;
  _entities.clear();
  logs::Logger::getInstance().info()
      << "Entity management system destroyed." << std::endl;
}

void CelteEntityManagementSystem::RegisterEntity(
    std::shared_ptr<celte::CelteEntity> entity) {
  // if the entity is already registered, we do nothing (it has already been
  // loaded from the network or by instantiating the scene)
  std::scoped_lock lock(_entitiesMutex);
  if (_entities.find(entity->GetUUID()) == _entities.end()) {
    _entities[entity->GetUUID()] = entity;
  }
}

void CelteEntityManagementSystem::UnregisterEntity(
    std::shared_ptr<celte::CelteEntity> entity) {
  std::scoped_lock lock(_entitiesMutex);
  auto it = _entities.find(entity->GetUUID());
  if (it != _entities.end()) {
    // we are not deleting the pointer because we do not own it.
    _entities.erase(it);
  }
}

celte::CelteEntity &
CelteEntityManagementSystem::GetEntity(const std::string &uuid) {
  std::scoped_lock lock(_entitiesMutex);
  return *_entities.at(uuid);
}

std::shared_ptr<CelteEntity>
CelteEntityManagementSystem::GetEntityPtr(const std::string &uuid) {
  std::scoped_lock lock(_entitiesMutex);
  auto it = _entities.find(uuid);
  if (it != _entities.end()) {
    return it->second;
  }
  return nullptr;
}

void CelteEntityManagementSystem::Tick() {
#ifdef CELTE_SERVER_MODE_ENABLED
  if (GRAPES.MustSendReplicationData()) {
    __replicateAllEntities();
    GRAPES.ResetReplicationDataTimer();
  }
#endif
}

#ifdef CELTE_SERVER_MODE_ENABLED
void CelteEntityManagementSystem::__replicateAllEntities() {
  for (auto &[uuid, entity] : _entities) {
    entity->UploadReplicationData();
  }
  GRAPES.ReplicateAllEntities();
}

std::string CelteEntityManagementSystem::GetRegisteredEntitiesSummary(
    const std::string &containerIdFilter) {
  nlohmann::json j = nlohmann::json::array();
  decltype(_entities) entities;
  {
    std::scoped_lock lock(_entitiesMutex);
    entities = _entities;
  }

  for (const auto &[uuid, entity] : entities) {
    try {
      if ((not containerIdFilter.empty()) and
          entity->GetOwnerChunk().GetCombinedId() != containerIdFilter) {
        continue;
      }

      // Create a new object for each entity
      nlohmann::json obj;

      obj["uuid"] = entity->GetUUID();
      obj["chunk"] = entity->GetOwnerChunk().GetCombinedId();
      obj["info"] = entity->GetInformationToLoad();
      std::string props = entity->GetProps();
      if (props.empty()) {
        obj["props"] = "";
      } else {
        obj["props"] = base64_encode(
            reinterpret_cast<const unsigned char *>(props.c_str()),
            props.size());
      }
      // Add the object to the JSON array
      j.push_back(obj);

    } catch (std::out_of_range &e) {
      continue; // entity is not associated with a chunk (just spawned,
                // initialisation not done yet)
    } catch (std::exception &e) {
      // If the entity is not associated with a chunk, log it
      std::cerr << "Error while packing entity " << entity->GetUUID()
                << " to json: " << e.what() << std::endl;
    }
  }
  return j.dump();
}
#endif

std::vector<std::string> CelteEntityManagementSystem::FilterEntities(
    const std::vector<std::string> &entityIds, const std::string &filter) {
  std::vector<std::string> result;
  bool (CelteEntityManagementSystem::*filterMethod)(const std::string &) const =
      _filters.at(filter);

  for (const auto &entityId : entityIds) {
    try {
      if ((this->*filterMethod)(entityId)) {
        result.push_back(entityId);
      }
    } catch (std::out_of_range &e) {
      logs::Logger::getInstance().err()
          << "Entity " << entityId << " not found." << std::endl;
    }
  }
  return result;
}

void CelteEntityManagementSystem::RegisterReplConsumer(
    const std::vector<std::string> &chunkId) {}

void CelteEntityManagementSystem::__handleReplicationDataReceived(
    std::unordered_map<std::string, std::string> &data, bool active) {
  // dropping extra headers
  data.erase(celte::tp::HEADER_PEER_UUID);
  for (auto &[entityId, blob] : data) {
    try {
      CelteEntity &entity = GetEntity(entityId);
      try {
        entity.DownloadReplicationData(blob);
#ifdef CELTE_SERVER_MODE_ENABLED
        HOOKS.server.replication.onReplicationDataReceived(entityId, blob);
#else
        HOOKS.client.replication.onReplicationDataReceived(entityId, blob);
#endif
      } catch (std::exception &e) {
        logs::Logger::getInstance().err()
            << "Error while downloading replication data: " << e.what()
            << std::endl;
      }
    } catch (std::out_of_range &e) {
      logs::Logger::getInstance().err()
          << "Entity " << entityId << " not found."
          << std::endl; // TODO: better handling for this, entities may need
                        // to spawn or smth
    }
  }
}

void CelteEntityManagementSystem::LoadExistingEntities(
    const std::string &summary) {
  std::vector<std::tuple<std::string, std::string>> containerAssigmentBatched;
  try {
    nlohmann::json summaryJSON = nlohmann::json::parse(summary);
    for (nlohmann::json &partialSummary : summaryJSON) {
      std::string uuid = partialSummary["uuid"];
      if (_entities.find(uuid) != _entities.end() or
          uuid == RUNTIME.GetUUID()) {
        continue; // entity already loaded
      }
      containerAssigmentBatched.push_back(
          std::make_tuple(uuid, partialSummary["chunk"]));
#ifdef CELTE_SERVER_MODE_ENABLED
      HOOKS.server.grape.onLoadExistingEntities(partialSummary);
#else
      HOOKS.client.grape.onLoadExistingEntities(partialSummary);
#endif
    }
  } catch (const std::exception &e) {
    logs::Logger::getInstance().err()
        << "Error loading existing entities: " << e.what() << std::endl;
    return;
  }
  // assigning entities to their respective containers
  for (auto &[uuid, chunkId] : containerAssigmentBatched) {
    RUNTIME.IO().post([this, uuid, chunkId]() {
      __takeLocallyDeferred(
          uuid, chunkId,
          std::chrono::system_clock::now() +
              std::chrono::seconds(10)); // timeout of one second
    });
  }
}

void CelteEntityManagementSystem::__takeLocallyDeferred(
    const std::string &uuid, const std::string &chunkId,
    std::chrono::time_point<std::chrono::system_clock> deadline) {
  if (std::chrono::system_clock::now() > deadline) {
    std::cerr << "Entity spawn timed out, won't spawn on the network"
              << std::endl;
    return;
  }
  if (not IsEntityRegistered(uuid)) {
    RUNTIME.IO().post([this, uuid, chunkId, deadline]() {
      __takeLocallyDeferred(uuid, chunkId, deadline);
    });
    return;
  }

  try {
    CelteEntity &entity = GetEntity(uuid);
    auto container = GRAPES.GetContainerById(chunkId);
    container->TakeEntityLocally(uuid);
  } catch (std::out_of_range &e) {
    std::cerr << "Error while taking entity " << uuid
              << " locally: " << e.what() << std::endl;
  }
}
} // namespace runtime
} // namespace celte