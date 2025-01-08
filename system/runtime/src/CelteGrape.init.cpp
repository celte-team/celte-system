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
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ServerStatesDeclaration.hpp"
#else
#include "ClientStatesDeclaration.hpp"
#endif

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
namespace chunks {

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

nlohmann::json Grape::FetchContainerFeatures() {
  if (_options.isLocallyOwned) {
    throw std::logic_error("Cannot fetch container features "
                           "from a locally owned grape.");
  }

  try {
    std::string response = _rpcs->Call<std::string>(
        tp::PERSIST_DEFAULT + _options.grapeId, "__rp_fetchContainerFeatures");
    // #ifdef CELTE_SERVER_MODE_ENABLED
    //     std::string response =
    //     server::states::ServerNet().rpcs().Call<std::string>(
    //         tp::PERSIST_DEFAULT + _options.grapeId,
    //         "__rp_fetchContainerFeatures");
    // #else
    //     std::string response =
    //     client::states::ClientNet().rpcs().Call<std::string>(
    //         tp::PERSIST_DEFAULT + _options.grapeId,
    //         "__rp_fetchContainerFeatures");
    // #endif

    return nlohmann::json::parse(response);
  } catch (net::RPCTimeoutException &e) {
    return nlohmann::json();
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::string Grape::__rp_fetchContainerFeatures() {
  nlohmann::json j;
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

void Grape::ForceUpdateContainer() {
  std::string featuresJSON = __rp_fetchContainerFeatures();
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId + "." + tp::RPCs,
                  "__rp_forceUpdateContainer", featuresJSON);
}

#endif

// we could just have the owner call this from its rpc channel
void Grape::__updateRemoteSubscriptions() {
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

void Grape::__rp_forceUpdateContainer(const std::string &containerFeatures) {
  try {
    nlohmann::json j = nlohmann::json::parse(containerFeatures);
    _rg.UpdateRemoteContainer(j["chunkId"], j);
  } catch (std::exception &e) {
    logs::Logger::getInstance().err()
        << "Error in ForceUpdateContainer: " << e.what();
  }
}

} // namespace chunks
} // namespace celte
