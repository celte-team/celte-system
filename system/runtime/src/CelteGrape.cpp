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
  _rg.RegisterOwnerGrapeId(_options.grapeId);
  _rg.SetInstantiateContainer([this]() {
    ChunkConfig config = {.grapeId = _options.grapeId,
                          .position = glm::ivec3(0),
                          .localX = _options.localX,
                          .localY = _options.localY,
                          .localZ = _options.localZ,
                          .size = _options.size,
                          .isLocallyOwned = _options.isLocallyOwned};
    auto chunk = std::make_shared<Chunk>(config);
    chunk->Initialize();
    _chunks[chunk->GetId()] = chunk;
    return chunk;
  });
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

Grape::~Grape() { std::cout << "Grape destructor called" << std::endl; }

void Grape::Initialize() {
  if (_options.subdivision <= 0) {
    throw std::invalid_argument("Subdivision must be a positive integer.");
  }
  logs::Logger::getInstance().info() << "Subdividing grape...";
  try {
    __initNetwork();
    __subdivide();
  } catch (std::exception &e) {
    logs::Logger::getInstance().err()
        << "Error, could not subdivide grape: " << e.what();
  }
  std::cout << "Grape has been created " << std::endl;
}

void Grape::__initNetwork() {
  std::vector<std::string> rpcTopics{tp::PERSIST_DEFAULT + _options.grapeId +
                                     "." + tp::RPCs};
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    std::cout << "Owning grape " << _options.grapeId << std::endl;
    rpcTopics.push_back(tp::PERSIST_DEFAULT + _options.grapeId);
  }
#endif

  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = rpcTopics,
      .reponseTopic = RUNTIME.GetUUID() + "." + tp::RPCs,
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

  _rpcs->Register<bool>(
      "__rp_spawnPlayer",
      std::function([this](std::string clientId, float x, float y, float z) {
        return __rp_spawnPlayer(clientId, x, y, z);
      }));
}

void Grape::__subdivide() {
  RotatedBoundingBox boundingBox(_options.position, _options.size,
                                 _options.localX, _options.localY,
                                 _options.localZ);
  auto points = boundingBox.GetMeshedPoints(_options.subdivision);
  std::cout << "creating " << points.size() << " chunks" << std::endl;

  for (auto point : points) {
    std::stringstream chunkId;
    glm::ivec3 pointInt = glm::ivec3(point);
    chunkId << pointInt.x << "-" << pointInt.y << "-" << pointInt.z;

    ChunkConfig config = {.chunkId = chunkId.str(),
                          .grapeId = _options.grapeId,
                          .position = pointInt,
                          .localX = _options.localX,
                          .localY = _options.localY,
                          .localZ = _options.localZ,
                          .size = _options.size / (float)_options.subdivision,
                          .isLocallyOwned = _options.isLocallyOwned};
    std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(config);
    _rg.RegisterEntityContainer(chunk);
    std::string combinedId = chunk->Initialize();
    _chunks[combinedId] = chunk;
  }

  RUNTIME.IO().post([this]() {
    // waiting until all readers are ready
    while (not _rpcs->Ready())
      ;
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
    if (not _options.isLocallyOwned) {
      _rpcs
          ->CallAsync<std::string>(tp::PERSIST_DEFAULT + _options.grapeId,
                                   "__rp_sendExistingEntitiesSummary",
                                   RUNTIME.GetUUID(), _options.grapeId)
          .Then([this](std::string summary) {
            ENTITIES.LoadExistingEntities(summary);
          });
    }
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
  ENTITIES.RemovePendingSpawn(clientId);
  std::cout << "on spawn requested grape " << _options.grapeId << std::endl;
  // spawn entity
  try {
#ifdef CELTE_SERVER_MODE_ENABLED
    HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
#else
    HOOKS.client.player.execPlayerSpawn(clientId, x, y, z);
#endif

  } catch (std::exception &e) {
    std::cerr << "Error in __rp_onSpawnRequested: " << e.what() << std::endl;
    return false;
  }
  __attachEntityAsync(clientId, x, y, z, 30);
  return true;
}

void Grape::__attachEntityAsync(std::string clientId, float x, float y, float z,
                                int retries) {
  if (not ENTITIES.IsEntityRegistered(clientId)) {
    if (retries <= 0) {
      std::cerr << "Entity spawn timed out, won't spawn on the network"
                << std::endl;
      return;
    }
    // RUNTIME.IO().post([this, clientId, x, y, z, retries]() {
    //   __attachEntityAsync(clientId, x, y, z, retries - 1);
    // });
    CLOCK.ScheduleAfter(10, [this, clientId, x, y, z, retries]() {
      __attachEntityAsync(clientId, x, y, z, retries - 1);
    });
    return;
  }

  auto &entity = ENTITIES.GetEntity(clientId);
  // std::shared_ptr<IEntityContainer> container =
  //     _rg.GetBestContainerForEntity(entity);
  std::optional<ReplicationGraph::ContainerAffinity> best =
      _rg.GetBestContainerForEntity(entity);
  if (not best.has_value()) {
    throw std::runtime_error(
        "No container has enough affinity with the entity");
  }

  // best->container->TakeEntityLocally(entity.GetUUID());
  _rg.TakeEntityLocally(entity.GetUUID(), best->container);
  best->container->SpawnEntityOnNetwork(entity.GetUUID(), x, y, z);
}
#endif

bool Grape::__rp_spawnPlayer(std::string clientId, float x, float y, float z) {
  // if entity already exist in the grape, do not spawn it again
  if (ENTITIES.IsEntityRegistered(clientId)) {
    return false;
  }
#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
#else
  HOOKS.client.player.execPlayerSpawn(clientId, x, y, z);
#endif

  return true;
}

Chunk &Grape::GetClosestChunk(float x, float y, float z) const {
  Chunk *closestChunk = nullptr;
  float closestDistance = std::numeric_limits<float>::max();
  for (auto &[chunkId, chunk] : _chunks) {
    float distance = chunk->GetDistanceToPosition(x, y, z);
    if (distance < closestDistance) {
      closestDistance = distance;
      closestChunk = &*chunk;
    }
  }
  if (closestChunk == nullptr) {
    throw std::out_of_range("No chunks in grape " + _options.grapeId);
  }
  return *closestChunk;
}

void Grape::Tick() {}

} // namespace chunks
} // namespace celte
