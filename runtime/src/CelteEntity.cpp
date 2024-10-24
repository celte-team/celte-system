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
    auto &chunk = chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
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

  logs::Logger::getInstance().info()
      << "Registering entity " << _uuid
      << " in the entity management system, with info " << _informationToLoad
      << std::endl;
  logs::Logger::getInstance().info().flush();
  ENTITIES.RegisterEntity(shared_from_this());
  _isSpawned = true; // will cause errors if OnSpawn is called but the entity is
                     // not actually spawned in the game.
}

void CelteEntity::OnDestroy() {
  ENTITIES.UnregisterEntity(shared_from_this());
  // TODO: Notify all peers of the destruction if in server mode and entity is
  // locally owned.
}

void CelteEntity::OnChunkTakeAuthority(celte::chunks::Chunk &chunk) {
  // _ownerChunk = const_cast<celte::chunks::Chunk *>(&chunk);
  logs::Logger::getInstance().info() << "on chunk take authority" << std::endl;
  logs::Logger::getInstance().info() << this << std::endl;
  logs::Logger::getInstance().info() << &chunk << std::endl;
  _ownerChunk = &chunk;
  logs::Logger::getInstance().info()
      << "Entity " << _uuid << " is now owned by chunk " << chunk.GetChunkId()
      << " in grape " << _ownerChunk->GetGrapeId() << std::endl;
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
    return;
  }

  _ownerChunk->ScheduleReplicationDataToSend(_uuid, _replicator.GetBlob());
}
#endif

const std::string &CelteEntity::GetInformationToLoad() const {
  return _informationToLoad;
}

} // namespace celte