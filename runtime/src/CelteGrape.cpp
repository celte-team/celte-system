#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace celte {
namespace chunks {

Grape::Grape(const GrapeOptions &options) : _options(options) {
  if (_options.subdivision <= 0) {
    throw std::invalid_argument("Subdivision must be a positive integer.");
  }
  logs::Logger::getInstance().info() << "Subdividing grape...";
  try {
    __initNetwork();
    __subdivide();
    logs::Logger::getInstance().info()
        << "Grape " << _options.grapeId << " created.";
  } catch (std::exception &e) {
    logs::Logger::getInstance().err()
        << "Error, could not subdivide grape: " << e.what();
  }
  logs::Logger::getInstance().info()
      << "Grape " << _options.grapeId << " created.";
}

Grape::Grape(Grape &grape, std::vector<std::string> chunksIds)
    : _options(grape._options) {
  throw std::logic_error(
      "Grape copy constructor not implemented, fix the options, grape id...");
  for (auto chunkId : chunksIds) {
    _chunks[chunkId] = grape._chunks[chunkId];
    grape._chunks.erase(chunkId);
  }
}

Grape::~Grape() {}

void Grape::__initNetwork() {
  std::vector<std::string> rpcTopics{tp::PERSIST_DEFAULT + _options.grapeId +
                                     "." + tp::RPCs};
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    rpcTopics.push_back(tp::PERSIST_DEFAULT + _options.grapeId);
  }
#endif

  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = rpcTopics,
      .serviceName =
          RUNTIME.GetUUID() + ".grape." + _options.grapeId + "." + tp::RPCs,
  });

  // todo: register rpcs specific to the grape

#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    _rpcs->Register<std::string>(
        "__rp_sendExistingEntitiesSummary",
        std::function([this](std::string clientId, std::string grapeId) {
          return ENTITIES.GetRegisteredEntitiesSummary();
        }));
  }

  _rpcs->Register<bool>(
      "__rp_onSpawnRequested", std::function([this](std::string clientId) {
        try {
          __rp_onSpawnRequested(clientId);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_onSpawnRequested: " << e.what()
                    << std::endl;
          return false;
        }
      }));
#endif
}

void Grape::__subdivide() {
  RotatedBoundingBox boundingBox(_options.position, _options.size,
                                 _options.localX, _options.localY,
                                 _options.localZ);
  auto points = boundingBox.GetMeshedPoints(_options.subdivision);

  for (auto point : points) {
    std::stringstream chunkId;
    glm::ivec3 pointInt = glm::ivec3(point);
    chunkId << "." << pointInt.x << "." << pointInt.y << "." << pointInt.z;
    ChunkConfig config = {.chunkId = chunkId.str(),
                          .grapeId = _options.grapeId,
                          .position = pointInt,
                          .localX = _options.localX,
                          .localY = _options.localY,
                          .localZ = _options.localZ,
                          .size = _options.size / (float)_options.subdivision,
                          .isLocallyOwned = _options.isLocallyOwned};
    std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(config);
    std::string combinedId = chunk->Initialize();
    _chunks[combinedId] = chunk;
  }

  RUNTIME.IO().post([this]() {
    // waiting until all readers are ready
    std::cout << "Waiting for readers to be ready..." << std::endl;
    while (not _rpcs->Ready())
      ;
    std::cout << "RPCs are ready." << std::endl;
    for (auto &[chunkId, chunk] : _chunks) {
      chunk->WaitNetworkInitialized();
    }
    std::cout << "All readers are ready." << std::endl;

    NET.PushThen([this]() {
      // calling user defined callback
      if (_options.then) {
        _options.then();
      }
    });

    // When ready, requesting the owner of the grape to send the existing
    // data to load on the grape
    std::cout << "Requesting existing entities summary..." << std::endl;
    if (not _options.isLocallyOwned) {
      std::cout << "Requesting" << std::endl;
      _rpcs
          ->CallAsync<std::string>(tp::PERSIST_DEFAULT + _options.grapeId,
                                   "__rp_sendExistingEntitiesSummary",
                                   RUNTIME.GetUUID(), _options.grapeId)
          .Then([this](std::string summary) {
            ENTITIES.LoadExistingEntities(_options.grapeId, summary);
          });
    }
    std::cout << "-----------------------------------" << std::endl;
  });
}

GrapeStatistics Grape::GetStatistics() const {
  GrapeStatistics stats = {.grapeId = _options.grapeId,
                           .numberOfChunks = _chunks.size()};
  for (auto &[chunkId, chunk] : _chunks) {
    stats.chunksIds.push_back(chunkId);
  }
  return stats;
}

bool Grape::ContainsPosition(float x, float y, float z) const {
  for (auto &[chunkId, chunk] : _chunks) {
    if (chunk->ContainsPosition(x, y, z)) {
      return true;
    }
  }
  return false;
}

Chunk &Grape::GetChunkByPosition(float x, float y, float z) {
  for (auto &[chunkId, chunk] : _chunks) {
    if (chunk->ContainsPosition(x, y, z)) {
      return *chunk;
    }
  }
  throw std::out_of_range("Position (" + std::to_string(x) + ", " +
                          std::to_string(y) + ", " + std::to_string(z) +
                          ") is not in grape " + _options.grapeId);
}

#ifdef CELTE_SERVER_MODE_ENABLED
void Grape::ReplicateAllEntities() {
  if (not _options.isLocallyOwned) {
    return;
  }
  for (auto &[chunkId, chunk] : _chunks) {
    chunk->SendReplicationData();
  }
}
#endif

bool Grape::HasChunk(const std::string &chunkId) const {
  return _chunks.find(chunkId) != _chunks.end();
}

Chunk &Grape::GetChunk(const std::string &chunkId) {
  if (not HasChunk(chunkId)) {
    throw std::out_of_range("Chunk " + chunkId + " does not exist in grape " +
                            _options.grapeId);
  }
  return *_chunks[chunkId];
}

#ifdef CELTE_SERVER_MODE_ENABLED
bool Grape::__rp_onSpawnRequested(std::string &clientId) {
  auto [_, x, y, z] = ENTITIES.GetPendingSpawn(clientId);
  try {
    auto &chunk =
        GRAPES.GetGrapeByPosition(x, y, z).GetChunkByPosition(x, y, z);
    chunk.SpawnPlayerOnNetwork(clientId, x, y, z);
    ENTITIES.RemovePendingSpawn(clientId);
    return true;
  } catch (std::out_of_range &e) {
    std::cerr << "Error in __rp_onSpawnRequested: " << e.what() << std::endl;
    return false;
  }
}
#endif

} // namespace chunks
} // namespace celte
