#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
// #include <boost/json.hpp>
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
    entity->ResetDataChanged();
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
      obj["passiveProps"] = entity->GetPassiveProps();
      obj["activeProprs"] = entity->GetActiveProps();

      // Add the object to the JSON array
      j.push_back(obj);
    } catch (std::out_of_range &e) {
      // If the entity is not associated with a chunk, log it
      logs::Logger::getInstance().err()
          << "Entity " << entity->GetUUID() << " is not owned by any chunk."
          << std::endl;
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
    const std::vector<std::string> &chunkId) {

  // for (auto &topic : chunkId) {
  //   KPOOL.RegisterTopicCallback(
  //       // Parsing the record to extract the new values of the properties,
  //       and
  //       // updating the entity
  //       topic,
  //       [this, topic](const kafka::clients::consumer::ConsumerRecord &record)
  //       {
  //         std::unordered_map<std::string, std::string> replData;
  //         for (const auto &header : record.headers()) {
  //           auto value =
  //               std::string(reinterpret_cast<const char
  //               *>(header.value.data()),
  //                           header.value.size());
  //           replData[header.key] = value;
  //         }
  //         bool active =
  //             (std::string(static_cast<const char *>(record.value().data()),
  //                          record.value().size())) == std::string("active");
  //         __handleReplicationDataReceived(replData, active);
  //       });
  // }
}

void CelteEntityManagementSystem::__handleReplicationDataReceived(
    std::unordered_map<std::string, std::string> &data, bool active) {
  // dropping extra headers
  data.erase(celte::tp::HEADER_PEER_UUID);
  for (auto &[entityId, blob] : data) {
    try {
      CelteEntity &entity = GetEntity(entityId);
      entity.DownloadReplicationData(blob, active);
#ifdef CELTE_SERVER_MODE_ENABLED
      if (active)
        HOOKS.server.replication.onActiveReplicationDataReceived(entityId,
                                                                 blob);
      else
        HOOKS.server.replication.onReplicationDataReceived(entityId, blob);
#else
      if (active)
        HOOKS.client.replication.onActiveReplicationDataReceived(entityId,
                                                                 blob);
      else
        HOOKS.client.replication.onReplicationDataReceived(entityId, blob);
#endif
    } catch (std::out_of_range &e) {
      logs::Logger::getInstance().err()
          << "Entity " << entityId << " not found."
          << std::endl; // TODO: better handling for this, entities may need to
                        // spawn or smth
    }
  }
}

void CelteEntityManagementSystem::LoadExistingEntities(
    const std::string &grapeId, const std::string &summary) {
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
      HOOKS.server.grape.onLoadExistingEntities(grapeId, partialSummary);
#else
      HOOKS.client.grape.onLoadExistingEntities(grapeId, partialSummary);
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