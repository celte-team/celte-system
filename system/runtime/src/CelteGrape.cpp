#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <chrono>
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
  std::string combinedId = chunk->Initialize();
  _chunks[combinedId] = chunk;
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
    rpcTopics.push_back(tp::PERSIST_DEFAULT + _options.grapeId);
  }
#endif

  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = rpcTopics,
      .responseTopic = RUNTIME.GetUUID() + "." + tp::RPCs,
      .serviceName =
          RUNTIME.GetUUID() + ".grape." + _options.grapeId + "." + tp::RPCs,
  });

#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    _rpcs->Register<std::string>("__rp_sendExistingEntitiesSummary",
                                 std::function([this](std::string chunkId) {
                                   return ENTITIES.GetRegisteredEntitiesSummary(
                                       chunkId);
                                 }));
  }

  _rpcs->Register<bool>(
      "__rp_onSpawnRequested",
      std::function([this](std::string clientId, std::string payload) {
        try {
          __rp_onSpawnRequested(clientId, payload);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_onSpawnRequested: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  _rpcs->Register<bool>(
      "__rp_remoteTakeEntity",
      std::function([this](std::string entityId, std::string callerId) {
        return __rp_remoteTakeEntity(entityId, callerId);
      }));

  _rpcs->Register<std::string>(
      "__rp_fetchContainerFeatures",
      std::function([this]() { return __rp_fetchContainerFeatures(); }));

#endif

  _rpcs->Register<bool>(
      "__rp_spawnEntity",
      std::function([this](std::string clientId, std::string containerId,
                           std::string payload, float x, float y, float z) {
        __execEntitySpawnProcess(clientId, containerId, payload, x, y, z);
        return true;
      }));

  _rpcs->Register<unsigned int>(
      "GetNumberOfContainers",
      std::function([this]() { return _rg.GetNumberOfContainers(); }));

  _rpcs->Register<bool>(
      "__rp_scheduleEntityAuthorityTransfer",
      std::function([this](std::string entityUUID, std::string newOwnerChunkId,
                           std::string newOwnerGrapeId, int tick) {
        __rp_scheduleEntityAuthorityTransfer(entityUUID, newOwnerChunkId,
                                             newOwnerGrapeId, tick);
        return true;
      }));
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

// executed on the node that drops
void Grape::RemoteTakeEntity(const std::string &entityId) {
  std::cout << "[[CALL REMOTE TAKE ENTITY]]" << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId,
                  "__rp_remoteTakeEntity", entityId, _options.grapeId);
}

// executed on the node that takes the entity
bool Grape::__rp_remoteTakeEntity(const std::string &entityId,
                                  const std::string &callerId) {
  try {
    std::cout << "[[remote take entity]]" << std::endl;

    RUNTIME.IO().post([this, entityId, callerId]() {
      // get the best container for the entity, or create it (can't refuse
      // entity)
      auto best = _rg.GetBestContainerForEntity(ENTITIES.GetEntity(entityId));
      if (not best.has_value()) {
        best = ReplicationGraph::ContainerAffinity{
            .container = _rg.AddContainer(),
            .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE};
      }
      best->container->WaitNetworkInitialized();
      ScheduleAuthorityTransfer(entityId, callerId, best->container->GetId());
    });
    return true;
  } catch (std::out_of_range &e) {
    std::cerr << "Error in __rp_remoteTakeEntity: " << e.what() << std::endl;
    return false;
  }
}

void Grape::ScheduleAuthorityTransfer(const std::string &entityId,
                                      const std::string &prevOwnerGrapeId,
                                      const std::string &newOwnerContainerId) {
  // we schedule the transfer in the
  // future to give time to everyone to
  // get ready
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + prevOwnerGrapeId + "." + tp::RPCs,
                  "__rp_"
                  "scheduleEntityAuthorityTransfer",
                  entityId, newOwnerContainerId, _options.grapeId,
                  CLOCK.CurrentTick() + _options.transferTickDelay);
}
#endif

// executed by everyone listening on the
// original owner's rpc channel (all peers
// listening to it)
void Grape::__rp_scheduleEntityAuthorityTransfer(
    const std::string &entityUUID, const std::string &newOwnerChunkId,
    const std::string &newOwnerGrapeId, int tick) {

  if (not ENTITIES.IsEntityRegistered(entityUUID)) {
    std::cerr << "Entity " << entityUUID
              << " not found, it is not "
                 "instantiated on this peer"
              << std::endl;
    return;
  }

  std::shared_ptr<Grape> newOwnerGrapePtr = GRAPES.GetGrapePtr(newOwnerGrapeId);
  if (newOwnerGrapePtr == nullptr) {
    // grape not replicated on this peer
    // TODO: @ewen destroy entity, it is
    // out of scope
    std::cout << "entity should be "
                 "destroyed here"
              << std::endl;
    return;
  }

  RUNTIME.IO().post([=]() {
    std::optional<std::shared_ptr<IEntityContainer>> newOwnerContainer =
        newOwnerGrapePtr->GetReplicationGraph().GetContainerOpt(
            newOwnerChunkId);
    if (not newOwnerContainer.has_value()) {
      newOwnerContainer =
          newOwnerGrapePtr->GetReplicationGraph().AddContainer(newOwnerChunkId);
    }

    CLOCK.ScheduleAt(tick, [this, entityUUID, newOwnerContainer]() {
      newOwnerContainer.value()->TakeEntityLocally(entityUUID);
    });
  });
}

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

void Grape::Tick() {
  if (not _options.isLocallyOwned) {
    __updateRemoteSubscriptions();
  }
}

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
bool Grape::__rp_onSpawnRequested(std::string &clientId, std::string &payload) {
  std::cout << "[[on spawn requested]]" << std::endl;
  try {
    auto [_, x, y, z] = ENTITIES.GetPendingSpawn(clientId);
    ENTITIES.RemovePendingSpawn(clientId);
    RUNTIME.IO().post([this, clientId, payload, x, y, z]() {
      __ownerExecEntitySpawnProcess(clientId, payload, x, y, z);
    });
  } catch (std::out_of_range &e) {
    std::cerr << "Error in "
                 "__rp_onSpawnRequested: "
              << e.what() << std::endl;
    return false;
  }
  return true;
}

void Grape::__ownerExecEntitySpawnProcess(const std::string &entityId,
                                          const std::string &payload, float x,
                                          float y, float z) {
  __spawnEntityLocally(
      entityId, payload, glm::vec3(x, y, z),
      [this, entityId, payload, x, y,
       z]() { // then, when godot is ready
        auto &entity = ENTITIES.GetEntity(entityId);
        entity.ExecInEngineLoop([this, &entity, entityId, payload, x, y, z]() {
          auto containerOpt = _rg.GetBestContainerForEntity(entity);
          std::shared_ptr<IEntityContainer> container;
          if (containerOpt.has_value()) {
            container = containerOpt.value().container;
          } else {
            auto newContainerAffinity = ReplicationGraph::ContainerAffinity{
                .container = _rg.AddContainer(),
                .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE};
            container = newContainerAffinity.container;
          }
          container->WaitNetworkInitialized();
          container->TakeEntityLocally(entityId);
          __spawnEntityOnNetwork(entityId, container->GetId(), payload, x, y,
                                 z);
        });
      });
}

void Grape::__spawnEntityOnNetwork(const std::string &entityId,
                                   const std::string &containerId,
                                   const std::string &payload, float x, float y,
                                   float z) {
  std::cout << "[[spawn entity on network]]" << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId + "." + tp::RPCs,
                  "__rp_spawnEntity", entityId, containerId, payload, x, y, z);
}

#endif

void Grape::__execEntitySpawnProcess(const std::string &entityId,
                                     const std::string &containerId,
                                     const std::string &payload, float x,
                                     float y, float z) {
  std::cout << "[[exec entity spawn process]]" << std::endl;
  if (_options.isLocallyOwned) {
    return; // done already in
            // ownerExecEntitySpawnProcess
  } else {
    __spawnEntityLocally(entityId, payload, glm::vec3(x, y, z),
                         [this, entityId, containerId, x, y, z]() {
                           auto containerOpt = _rg.GetContainerOpt(containerId);
                           std::shared_ptr<IEntityContainer> container;
                           if (containerOpt.has_value()) {
                             container = containerOpt.value();
                           } else {
                             container = _rg.AddContainer(containerId);
                           }
                           container->WaitNetworkInitialized();
                           container->TakeEntityLocally(entityId);
                         });
  }
}

void Grape::__spawnEntityLocally(const std::string &entityId,
                                 const std::string &payload, glm::vec3 position,
                                 std::function<void()> then) {
  std::cout << "[[spawn entity locally]]" << std::endl;
  __callSpawnHook(entityId, payload, position);
  __waitEntityReady(entityId, then);
}

void Grape::__waitEntityReady(const std::string &entityId,
                              std::function<void()> then) {
  if (not ENTITIES.IsEntityRegistered(entityId)) {
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

void Grape::__callSpawnHook(const std::string &entityId,
                            const std::string &payload, glm::vec3 position) {
  std::cout << "[[call spawn hook]]" << std::endl;
  try {
#ifdef CELTE_SERVER_MODE_ENABLED
    HOOKS.server.newPlayerConnected.execPlayerSpawn(
        entityId, payload, position.x, position.y, position.z);
#else
    HOOKS.client.player.execPlayerSpawn(entityId, payload, position.x,
                                        position.y, position.z);
#endif
  } catch (std::exception &e) {
    std::cerr << "Error in __callSpawnHook: " << e.what() << std::endl;
  }
}

#pragma endregion spawnprocess

#pragma region remote_grapes_communication

nlohmann::json Grape::FetchContainerFeatures() {
  if (_options.isLocallyOwned) {
    throw std::logic_error("Cannot fetch container features "
                           "from a locally owned grape.");
  }

  try {
    std::string response = _rpcs->Call<std::string>(
        tp::PERSIST_DEFAULT + _options.grapeId, "__rp_fetchContainerFeatures");
    return nlohmann::json::parse(response);
  } catch (net::RPCTimeoutException &e) {
    return nlohmann::json();
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::string Grape::__rp_fetchContainerFeatures() {
  nlohmann::json j;
  if (not _options.isLocallyOwned) {
    throw std::logic_error("Cannot fetch container features "
                           "from a locally owned grape.");
  }
  std::unordered_map<std::string, std::shared_ptr<IEntityContainer>>
      containers = _rg.GetContainers();
  for (auto &[_, container] : containers) {
    nlohmann::json containerJson;
    containerJson["features"] = container->GetFeatures();
    containerJson["config"] = container->GetConfigJSON();
    j[container->GetId()] = containerJson;
  }
  return j.dump();
}

#endif
void Grape::__updateRemoteSubscriptions() { // maybe
                                            // the
                                            // owner
                                            // grape
                                            // could
                                            // just
                                            // call
                                            // this
                                            // on
                                            // its
                                            // rpc
                                            // channel

  auto now = std::chrono::system_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now -
                                                       _lastRemoteSubUpdate)
          .count() < 1) {
    return;
  }
  _lastRemoteSubUpdate = now;

  RUNTIME.IO().post([this]() {
    nlohmann::json j = FetchContainerFeatures();
    if (j.size() == 0) {
      return;
    }
    for (auto &[containerId, info] : j.items()) {
      _rg.UpdateRemoteContainer(containerId, info);
    }
  });
}

#pragma endregion remote_grapes_communication

} // namespace chunks
} // namespace celte
