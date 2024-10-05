#include "CelteEntityManagementSystem.hpp"
#include "CelteRPC.hpp"

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

} // namespace runtime
} // namespace celte