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
  if (_entities.find(entity->GetUUID()) == _entities.end()) {
    _entities[entity->GetUUID()] = entity;
  }
}

void CelteEntityManagementSystem::UnregisterEntity(
    std::shared_ptr<celte::CelteEntity> entity) {
  auto it = _entities.find(entity->GetUUID());
  if (it != _entities.end()) {
    // we are not deleting the pointer because we do not own it.
    _entities.erase(it);
  }
}

celte::CelteEntity &
CelteEntityManagementSystem::GetEntity(const std::string &uuid) const {
  return *_entities.at(uuid);
}

std::shared_ptr<CelteEntity>
CelteEntityManagementSystem::GetEntityPtr(const std::string &uuid) const {
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

// std::string CelteEntityManagementSystem::GetRegisteredEntitiesSummary() {
//   /*
//   Format is :
//   [
//     {
//       "uuid": "uuid",
//       "chunk": "chunkCombinedId",
//       "info": "info"
//     },
//     {
//       "uuid": "uuid",
//       "chunk": "chunkCombinedId",
//       "info": "info"
//     }
//   ]
//   */
//   boost::json::array j;

//   for (const auto &[uuid, entity] : _entities) {
//     try {
//       logs::Logger::getInstance().info()
//           << "packing entity " << uuid << " to json." << std::endl;
//       // Create a new object for each entity
//       boost::json::object obj;

//       obj["uuid"] = entity->GetUUID();
//       obj["chunk"] = entity->GetOwnerChunk().GetCombinedId();
//       obj["info"] = entity->GetInformationToLoad();

//       // Add the object to the JSON array
//       j.push_back(obj);
//     } catch (std::out_of_range &e) {
//       // If the entity is not associated with a chunk, log it
//       logs::Logger::getInstance().err()
//           << "Entity " << entity->GetUUID() << " is not owned by any chunk."
//           << std::endl;
//     }
//   }

//   return boost::json::serialize(j);
// }

std::string CelteEntityManagementSystem::GetRegisteredEntitiesSummary() {
  /*
  Format is :
  [
    {
      "uuid": "uuid",
      "chunk": "chunkCombinedId",
      "info": "info"
    },
    {
      "uuid": "uuid",
      "chunk": "chunkCombinedId",
      "info": "info"
    }
  ]
  */
  nlohmann::json j = nlohmann::json::array();

  for (const auto &[uuid, entity] : _entities) {
    try {
      if (not entity->GetOwnerChunk().GetConfig().isLocallyOwned) {
        continue;
      }
      logs::Logger::getInstance().info()
          << "packing entity " << uuid << " to json." << std::endl;
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
    } catch (std::exception &e) {
      // If the entity is not associated with a chunk, log it
      std::cerr << "Error while packing entity " << entity->GetUUID()
                << " to json: " << e.what() << std::endl;
    }
  }
  std::cout << "Returning json" << std::endl;
  std::cout << j.dump() << std::endl;
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
  std::cout << "In LoadExistingEntities" << std::endl;
  try {
    nlohmann::json summaryJSON = nlohmann::json::parse(summary);

    for (nlohmann::json &partialSummary : summaryJSON) {
      std::string uuid = partialSummary["uuid"];
      std::cout << "entity uuid: " << uuid << std::endl;
      if (_entities.find(uuid) != _entities.end() or
          uuid == RUNTIME.GetUUID()) {
        continue; // entity already loaded
      }
      std::cout << "entity not found" << std::endl;
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
}

} // namespace runtime
} // namespace celte