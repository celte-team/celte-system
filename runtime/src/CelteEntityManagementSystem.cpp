#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"

namespace celte {
namespace runtime {
void CelteEntityManagementSystem::RegisterEntity(
    const celte::CelteEntity &entity) {
  _entities[entity.GetUUID()] = new celte::CelteEntity(entity);
}

void CelteEntityManagementSystem::UnregisterEntity(
    const celte::CelteEntity &entity) {
  auto it = _entities.find(entity.GetUUID());
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
#endif

} // namespace runtime
} // namespace celte