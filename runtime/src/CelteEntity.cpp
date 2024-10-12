#include "CelteChunk.hpp"
#include "CelteEntity.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
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
    RUNTIME.Err() << "Entity is not in any grape: " << e.what() << std::endl;
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
  // TODO: Notify all peers of the destruction if in server mode and entity is
  // locally owned.
}

void CelteEntity::OnChunkTakeAuthority(const celte::chunks::Chunk &chunk) {
  _ownerChunk = const_cast<celte::chunks::Chunk *>(&chunk);
}

void CelteEntity::Tick() {
  // nothing yet :)
}

#ifdef CELTE_SERVER_MODE_ENABLED
void CelteEntity::UploadReplicationData() {

  if (not(_ownerChunk and GRAPES.GetGrape(_ownerChunk->GetGrapeId())
                              .GetOptions()
                              .isLocallyOwned)) {
    return;
  }

  if (not _ownerChunk) {
    logs::Logger::getInstance().err()
        << "Entity " << _uuid << " is not owned by any chunk." << std::endl;
    return;
  }

  _ownerChunk->ScheduleReplicationDataToSend(_uuid, _replicator.GetBlob());
}
#endif

} // namespace celte