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
  for (auto chunkId : chunksIds) {
    _chunks[chunkId] = grape._chunks[chunkId];
    grape._chunks.erase(chunkId);
  }
}

Grape::~Grape() {}

void Grape::__subdivide() {
  // subdivide each axis into _options.subdivision parts to create a list of
  // points equally spaced along each axis, to map the space in the grape
  RotatedBoundingBox boundingBox(_options.position, _options.size,
                                 _options.localX, _options.localY,
                                 _options.localZ);
  auto points = boundingBox.GetMeshedPoints(_options.subdivision);

  std::vector<std::string> rpcTopics;
  std::vector<std::string> replTopics;

  // create a chunk for each point
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

    std::cout << "GRAPE [" << _options.grapeId << "] CHUNK [" << chunkId.str()
              << "]" << std::endl;
    std::cout << "registering rpcs" << std::endl;
    std::shared_ptr<Chunk> chunk = std::make_shared<Chunk>(config);
    std::string combinedId = chunk->Initialize();
    _chunks[combinedId] = chunk;

    rpcTopics.push_back(combinedId + "." + tp::RPCs);
    replTopics.push_back(combinedId + "." + tp::REPLICATION);
  }

// Server creates replication topics
#ifdef CELTE_SERVER_MODE_ENABLED
  KPOOL.CreateTopicsIfNotExist(replTopics, 1, 1);
  KPOOL.CreateTopicsIfNotExist(rpcTopics, 1, 1);
  if (_options.isLocallyOwned) {
    std::vector<std::string> grapeTopic = {_options.grapeId};
    KPOOL.CreateTopicsIfNotExist(grapeTopic, 1, 1);
    KPOOL.RegisterTopicCallback(
        _options.grapeId,
        [this](const kafka::clients::consumer::ConsumerRecord &record) {
          RPC.InvokeLocal(record);
        });
  }
#endif

  std::vector<std::string> topics;
  topics.insert(topics.end(), rpcTopics.begin(), rpcTopics.end());
  if (not _options.isLocallyOwned) {
    topics.insert(topics.end(), replTopics.begin(), replTopics.end());
    ENTITIES.RegisterReplConsumer(replTopics);
  }

  std::function<void()> then =
      (_options.then != nullptr) ? _options.then : nullptr;
  if (not _options.isLocallyOwned) {
    then = [this]() {
      // request the SN managing the node to udpate us with the data we need to
      // load the existing entities in the grape
      std::cout << "requesting existing entities summary" << std::endl;
      RPC.InvokeByTopic(_options.grapeId, "__rp_sendExistingEntitiesSummary",
                        RUNTIME.GetUUID(), _options.grapeId);
      if (_options.then != nullptr) {
        _options.then();
      }
    };
  } else {
    topics.push_back(_options.grapeId);
  }

  KPOOL.Subscribe({
      .topics = topics, .autoCreateTopic = false, .then = then,
      // callbacks are already set in the chunk Initialize method and
      // ENTITIES.RegisterReplConsumer
  });
  KPOOL.CommitSubscriptions();
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

} // namespace chunks
} // namespace celte
