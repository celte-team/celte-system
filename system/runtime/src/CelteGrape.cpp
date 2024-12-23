#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
namespace chunks {

#pragma region init
Grape::Grape(const GrapeOptions &options) : _options(options) {
  _rg.RegisterOwnerGrapeId(_options.grapeId);
  _rg.SetInstantiateContainer([this](const std::string &containerId) {
    return __defaultInstantiateContainer(containerId);
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
  try {
    __initNetwork();
    __subdivide();
  } catch (std::exception &e) {
    logs::Logger::getInstance().err()
        << "Error, could not subdivide grape: " << e.what();
  }
}

void Grape::__subdivide() {
  RotatedBoundingBox boundingBox(_options.position, _options.size,
                                 _options.localX, _options.localY,
                                 _options.localZ);
  RUNTIME.IO().post([this]() {
    // waiting until all readers are ready
    while (not _rpcs->Ready())
      ;

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

std::shared_ptr<IEntityContainer>
Grape::__defaultInstantiateContainer(const std::string &containerId) {

  nlohmann::json config = {
      {"chunkId", containerId},
      {"grapeId", _options.grapeId},
      {"preferredEntityCount", 100},
      {"preferredContainerSize", 10},
      {"isLocallyOwned", _options.isLocallyOwned},
  };

  std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(config);
  chunk->SetEntityPositionGetter([this](const std::string &entityId) {
    if (_entityPositionGetter == nullptr) {
      std::cerr << "Entity position getter not set, cannot get position"
                << std::endl;
      return glm::vec3(0);
    }
    return _entityPositionGetter(entityId);
  });
  // _rg.RegisterEntityContainer(chunk);// this is a deadlock
  std::cout << "[[chunk initialize]]" << std::endl;
  std::string combinedId = chunk->Initialize();
  _chunks[combinedId] = chunk;
  std::cout << "default instantiate container returning chunk" << std::endl;
  return chunk;
}

GrapeStatistics Grape::GetStatistics() const {
  GrapeStatistics stats = {.grapeId = _options.grapeId,
                           .numberOfChunks = _chunks.size()};
  for (auto &[chunkId, chunk] : _chunks) {
    stats.chunksIds.push_back(chunkId);
  }
  return stats;
}

#pragma endregion init

#pragma region network
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
      "__rp_spawnEntity",
      std::function([this](std::string clientId, std::string containerId,
                           float x, float y, float z) {
        __execEntitySpawnProcess(clientId, containerId, x, y, z);
        return true;
      }));

  _rpcs->Register<unsigned int>(
      "GetNumberOfContainers",
      std::function([this]() { return _rg.GetNumberOfContainers(); }));
}

#pragma endregion network

#pragma region runtime_logic_and_getters
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

void Grape::Tick() {}

nlohmann::json Grape::Dump() const {
  nlohmann::json j;
  j["locally owned"] = _options.isLocallyOwned;

  nlohmann::json replicationDump = _rg.Dump();
  j["replication graph"] = replicationDump;
  return j;
}
#pragma endregion runtime_logic_and_getters

#pragma region spawnprocess

#ifdef CELTE_SERVER_MODE_ENABLED
bool Grape::__rp_onSpawnRequested(std::string &clientId) {
  std::cout << "[[on spawn requested]]" << std::endl;
  try {
    auto [_, x, y, z] = ENTITIES.GetPendingSpawn(clientId);
    ENTITIES.RemovePendingSpawn(clientId);
    RUNTIME.IO().post([this, clientId, x, y, z]() {
      __ownerExecEntitySpawnProcess(clientId, x, y, z);
    });
  } catch (std::out_of_range &e) {
    std::cerr << "Error in __rp_onSpawnRequested: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void Grape::__ownerExecEntitySpawnProcess(const std::string &entityId, float x,
                                          float y, float z) {
  std::cout << "[[owner exec entity spawn process]]" << std::endl;
  __spawnEntityLocally(
      entityId, glm::vec3(x, y, z), [this, entityId, x, y, z]() {
        auto &entity = ENTITIES.GetEntity(entityId);
        entity.ExecInEngineLoop([this, &entity, entityId, x, y, z]() {
          std::shared_ptr<IEntityContainer> container =
              _rg.GetBestContainerForEntity(entity)
                  .value_or(ReplicationGraph::ContainerAffinity{
                      .container = _rg.AddContainer(),
                      .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE})
                  .container;
          container->WaitNetworkInitialized();
          container->TakeEntityLocally(entityId);
          __spawnEntityOnNetwork(entityId, container->GetId(), x, y, z);
        });
      });
}

void Grape::__spawnEntityOnNetwork(const std::string &entityId,
                                   const std::string &containerId, float x,
                                   float y, float z) {
  std::cout << "[[spawn entity on network]]" << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId + "." + tp::RPCs,
                  "__rp_spawnEntity", entityId, containerId, x, y, z);
}

#endif

void Grape::__execEntitySpawnProcess(const std::string &entityId,
                                     const std::string &containerId, float x,
                                     float y, float z) {
  std::cout << "[[exec entity spawn process]]" << std::endl;
  if (_options.isLocallyOwned) {
    return; // done already in ownerExecEntitySpawnProcess
  } else {
    __spawnEntityLocally(entityId, glm::vec3(x, y, z),
                         [this, entityId, containerId, x, y, z]() {
                           std::shared_ptr<IEntityContainer> container =
                               _rg.GetContainerOpt(containerId)
                                   .value_or(_rg.AddContainer(containerId));
                           container->WaitNetworkInitialized();
                           container->TakeEntityLocally(entityId);
                         });
  }
}

void Grape::__spawnEntityLocally(const std::string &entityId,
                                 glm::vec3 position,
                                 std::function<void()> then) {
  std::cout << "[[spawn entity locally]]" << std::endl;
  __callSpawnHook(entityId, position);
  __waitEntityReady(entityId, then);
}

void Grape::__waitEntityReady(const std::string &entityId,
                              std::function<void()> then) {
  std::cout << "[[wait entity ready]]" << std::endl;
  if (not ENTITIES.IsEntityRegistered(entityId)) {
    std::cout << "no ready... waiting" << std::endl;
    RUNTIME.IO().post(
        [this, entityId, then]() { __waitEntityReady(entityId, then); });
    return;
  }
  try {
    then();
  } catch (std::exception &e) {
    std::cerr << "Error in __waitEntityReady: " << e.what() << std::endl;
  }
}

void Grape::__callSpawnHook(const std::string &entityId, glm::vec3 position) {
  std::cout << "[[call spawn hook]]" << std::endl;
  try {
#ifdef CELTE_SERVER_MODE_ENABLED
    HOOKS.server.newPlayerConnected.execPlayerSpawn(entityId, position.x,
                                                    position.y, position.z);
#else
    HOOKS.client.player.execPlayerSpawn(entityId, position.x, position.y,
                                        position.z);
#endif
  } catch (std::exception &e) {
    std::cerr << "Error in __callSpawnHook: " << e.what() << std::endl;
  }
}

#pragma endregion spawnprocess

#pragma region remote_grapes_communication

nlohmann::json Grape::FetchContainerFeatures() {
  if (_options.isLocallyOwned) {
    throw std::logic_error(
        "Cannot fetch container features from a locally owned grape.");
  }

  std::string response = _rpcs->Call<std::string>(
      tp::PERSIST_DEFAULT + _options.grapeId, "__rp_fetchContainerFeatures");

  return nlohmann::json::parse(response);
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::string Grape::__rp_fetchContainerFeatures() {
  nlohmann::json j;
  if (not _options.isLocallyOwned) {
    throw std::logic_error(
        "Cannot fetch container features from a locally owned grape.");
  }
  std::vector<std::shared_ptr<IEntityContainer>> containers =
      _rg.GetContainers();
  for (auto &container : containers) {
    j[container->GetId()] = container->GetFeatures();
  }
  return j.dump();
}
#endif

#pragma endregion remote_grapes_communication

} // namespace chunks
} // namespace celte
