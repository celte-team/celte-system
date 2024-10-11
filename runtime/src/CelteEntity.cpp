#include "CelteChunk.hpp"
#include "CelteEntity.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

namespace celte {
void CelteEntity::OnSpawn(float x, float y, float z, const std::string &uuid) {
  try {
    auto chunk = chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
                     .GetGrapeByPosition(x, y, z)
                     .GetChunkByPosition(x, y, z);
    OnChunkTakeAuthority(chunk);
  } catch (std::out_of_range &e) {
    // Entity is not in any grape
    std::cerr << "Entity is not in any grape: " << e.what() << std::endl;
  }

  if (uuid.empty()) {
    _uuid = boost::uuids::to_string(boost::uuids::random_generator()());
  } else {
    _uuid = uuid;
  }
  ENTITIES.RegisterEntity(*this);
}

void CelteEntity::OnDestroy() {
  ENTITIES.UnregisterEntity(*this);
  // TODO: Notify all peers of the destruction
}

void CelteEntity::OnChunkTakeAuthority(const celte::chunks::Chunk &chunk) {
  _ownerChunk = const_cast<celte::chunks::Chunk *>(&chunk);
}

void CelteEntity::Tick() {
  // if the chunk where the entity is is locally owned, we upload the data of
  // the current simulation's results.
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_ownerChunk and
      GRAPES.GetGrape(_ownerChunk->GetGrapeId()).GetOptions().isLocallyOwned) {
    __uploadReplicationData();
  }
#endif
}

#ifdef CELTE_SERVER_MODE_ENABLED
void CelteEntity::__uploadReplicationData() {
  // TODO
}
#endif

} // namespace celte