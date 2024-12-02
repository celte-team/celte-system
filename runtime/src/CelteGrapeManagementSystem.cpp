#include "CelteGrapeManagementSystem.hpp"
#include "Logger.hpp"
#include <sstream>
#include <stdexcept>

namespace celte {
namespace chunks {
Grape &CelteGrapeManagementSystem::RegisterGrape(const GrapeOptions &options) {
  auto grape = std::make_shared<Grape>(options);
  _grapes[grape->GetGrapeId()] = grape;
  return *grape;
}

CelteGrapeManagementSystem &CelteGrapeManagementSystem::GRAPE_MANAGER() {
  static CelteGrapeManagementSystem instance;
  return instance;
}

Grape &CelteGrapeManagementSystem::GetGrape(std::string grapeId) {
  if (_grapes.find(grapeId) == _grapes.end()) {
    throw std::out_of_range("Grape with id " + grapeId + " does not exist.");
  }
  return *_grapes[grapeId];
}

Grape &CelteGrapeManagementSystem::GetGrapeByPosition(float x, float y,
                                                      float z) {
  for (auto &[grapeId, grape] : _grapes) {
    if (grape->ContainsPosition(x, y, z)) {
      return *grape;
    }
  }
  std::stringstream ss;
  ss << "No grape contains the position (" << x << ", " << y << ", " << z
     << ").";
  ss << "List of the grapes searched for : \n";
  for (auto &[grapeId, grape] : _grapes) {
    ss << grapeId << "\n";
  }
  throw std::out_of_range(ss.str());
}

Chunk &CelteGrapeManagementSystem::GetChunkById(const std::string &chunkId) {
  for (auto &[grapeId, grape] : _grapes) {
    if (grape->HasChunk(chunkId)) {
      return grape->GetChunk(chunkId);
    }
  }
  throw std::out_of_range("No chunk with id " + chunkId + " exists.");
}

#ifdef CELTE_SERVER_MODE_ENABLED
void CelteGrapeManagementSystem::ReplicateAllEntities() {
  for (auto &[grapeId, grape] : _grapes) {
    grape->ReplicateAllEntities();
  }
}
#endif
} // namespace chunks
} // namespace celte