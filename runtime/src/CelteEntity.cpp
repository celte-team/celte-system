#include "CelteEntity.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

namespace celte {
void CelteEntity::OnSpawn(float x, float y, float z) {
  try {
    auto chunk = chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
                     .GetGrapeByPosition(x, y, z)
                     .GetChunkByPosition(x, y, z);
    OnChunkTakeAuthority(chunk);
  } catch (std::out_of_range &e) {
    // Entity is not in any grape
    std::cerr << "Entity is not in any grape: " << e.what() << std::endl;
  }

  _uuid = boost::uuids::to_string(boost::uuids::random_generator()());
  ENTITIES.RegisterEntity(*this);
}

void CelteEntity::OnDestroy() {
  ENTITIES.UnregisterEntity(*this);
  // TODO: Notify all peers of the destruction
}

void CelteEntity::OnChunkTakeAuthority(const celte::chunks::Chunk &chunk) {
  _ownerChunk = const_cast<celte::chunks::Chunk *>(&chunk);
}

} // namespace celte