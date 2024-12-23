#include "CelteGrapeManagementSystem.hpp"
#include "Logger.hpp"
#include "nlohmann/json.hpp"
#include <sstream>
#include <stdexcept>

namespace celte {
namespace chunks {
Grape &CelteGrapeManagementSystem::RegisterGrape(const GrapeOptions &options) {
  _grapes.insert({options.grapeId, std::make_shared<Grape>(options)});
  return *_grapes[options.grapeId];
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

std::shared_ptr<IEntityContainer> CelteGrapeManagementSystem::GetContainerById(
    const std::string &containerId) { // TODO: store a global map of containers
                                      // for faster access
  for (auto &[grapeId, grape] : _grapes) {
    auto container = grape->GetReplicationGraph().GetContainerById(containerId);
  }
  throw std::out_of_range("No container with id " + containerId + " exists.");
}

std::vector<std::shared_ptr<Grape>> CelteGrapeManagementSystem::GetGrapes() {
  std::vector<std::shared_ptr<Grape>> grapes;
  for (auto &[grapeId, grape] : _grapes) {
    grapes.push_back(grape);
  }
  return grapes;
}

std::string CelteGrapeManagementSystem::DumpGrapes() const {
  nlohmann::json j;
  for (auto &[grapeId, grape] : _grapes) {
    j[grapeId] = grape->Dump();
  }
  return j.dump();
}

} // namespace chunks
} // namespace celte