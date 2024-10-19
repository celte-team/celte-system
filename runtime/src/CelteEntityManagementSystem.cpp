#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include <boost/json.hpp>

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
  // for (auto &[uuid, entity] : _entities) {
  //   entity->UploadReplicationData();
  //   entity->ResetDataChanged();
  // }
  // GRAPES.ReplicateAllEntities();
}

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
  boost::json::array j;

  for (const auto &[uuid, entity] : _entities) {
    try {
      logs::Logger::getInstance().info()
          << "packing entity " << uuid << " to json." << std::endl;
      // Create a new object for each entity
      boost::json::object obj;

      obj["uuid"] = entity->GetUUID();
      obj["chunk"] = entity->GetOwnerChunk().GetCombinedId();
      obj["info"] = entity->GetInformationToLoad();

      // Add the object to the JSON array
      j.push_back(obj);
    } catch (std::out_of_range &e) {
      // If the entity is not associated with a chunk, log it
      logs::Logger::getInstance().err()
          << "Entity " << entity->GetUUID() << " is not owned by any chunk."
          << std::endl;
    }
  }

  return boost::json::serialize(j);
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
} // namespace runtime
} // namespace celte