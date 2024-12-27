#pragma once
#include "CelteChunk.hpp"
#include "CelteService.hpp"
#include "RPCService.hpp"
#include "ReplicationGraph.hpp"
#include "RotatedBoundingBox.hpp"
#include "WriterStreamPool.hpp"
#include <chrono>
#include <functional>
#include <glm/vec3.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace celte {
namespace chunks {
struct TransferInfo {
  std::string entityId;
  std::string gFrom;
  std::string cFrom;
  std::string gTo;
  std::string cTo;
};

/**
 * Use these options to configure a grape.
 * These options should be available in the engine's editor,
 * wrapped by a class in the engine's celte api.
 */
struct GrapeOptions {
  std::string grapeId;
  const int subdivision = 1;
  const glm::vec3 size;
  const glm::vec3 localX;
  const glm::vec3 localY;
  const glm::vec3 localZ;
  // The center of the grape
  const glm::vec3 position;
  // is this grape owned by the local node?
  bool isLocallyOwned = false;
  int transferTickDelay = 30;
  std::function<void()> then = nullptr;
};

struct GrapeStatistics {
  const std::string grapeId;
  const size_t numberOfChunks;
  std::vector<std::string> chunksIds;
};

/**
 * @brief A grape is a region of the world that is managed by a single
 * server node. Grapes are subdivided into
 * chunks to allow reassigning overloaded areas to new server nodes.
 * This is doable by using the grape constructor that takes a grape as
 * an argument.
 */
class Grape {
public:
  Grape(const GrapeOptions &options);
  void Initialize();

  inline bool IsLocallyOwned() const { return _options.isLocallyOwned; }

  /**
   * @brief This method should be called at each tick of the game loop, in the
   * game engine Celte API.
   */
  void Tick();

  /**
   * @brief Construct a grape from another grape.
   * This constructor is used to create a new grape that is a
   * subdivision of an existing grape. The chunks of the new grape
   * will be removed from the previous grape.
   *
   * @param grape: The grape to subdivide
   * @param chunks: The chunks to remove from the previous grape and
   * add to the new one.
   */
  Grape(Grape &grape, std::vector<std::string> chunksIds);
  ~Grape();

  /**
   * @brief Returns statistics and informations about the Grape.
   * This method is used to display information about the grape in the
   * engine's editor or for debugging purposes.
   */
  GrapeStatistics GetStatistics() const;

  inline glm::vec3 GetPosition() const { return _options.position; }
  inline glm::vec3 GetSize() const { return _options.size; }

  /**
   * @brief Returns the id of the grape.
   */
  inline const std::string &GetGrapeId() const { return _options.grapeId; }

  /**
   * @brief Returns the options used to create the grape.
   */
  inline const GrapeOptions &GetOptions() const { return _options; }

  inline const std::vector<std::string> GetChunksIds() const {
    std::vector<std::string> chunksIds;
    for (auto &[chunkId, _] : _chunks) {
      chunksIds.push_back(chunkId);
    }
    return chunksIds;
  }

  bool HasChunk(const std::string &chunkId) const;

  Chunk &GetChunk(const std::string &chunkId);

  /**
   * @brief Returns the number of entity containers in the grape.
   * @warning This method will fetch this information over the network if not
   * available, so use it asynchronously if possible.
   */
  inline unsigned int GetNumberOfContainers() const {
    return _rg.GetNumberOfContainers();
  }

  /**
   * @brief Returns a json string containing information about the current state
   * of the grape.
   */
  nlohmann::json Dump() const;

  void inline SetEntityPositionGetter(
      std::function<glm::vec3(const std::string &)> getter) {
    _entityPositionGetter = getter;
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Replicates all entities in the grape to their respective chunks.
   */
  void ReplicateAllEntities();

  void TransferAuthority(const TransferInfo &ti);

  inline void TakeEntity(const std::string &entityId) {
    _rg.TakeEntity(entityId);
  }

  void RemoteTakeEntity(const std::string &entityId);
#endif

  /**
   * @brief Returns a handle to the rpc service of the grape.
   *
   * @throws std::runtime_error if the rpc service has not been initialized yet.
   */
  inline net::RPCService &GetRPCService() {
    if (not _rpcs.has_value()) {
      throw std::runtime_error("RPC service not initialized yet");
    }
    return _rpcs.value();
  }

  /**
   * @brief Returns a handle to the rpc service of the chunk with the given
   * id.api
   *
   * @throws std::out_of_range if the chunk does not exist.
   * @throws std::runtime_error if the rpc service has not been initialized yet.
   */
  inline net::RPCService &GetChunkRPCService(const std::string &chunkId) {
    return GetChunk(chunkId).GetRPCService();
  }

  inline ReplicationGraph &GetReplicationGraph() { return _rg; }

  /**
   * @brief Fetches the features of all containers owned by the grape.
   */
  nlohmann::json FetchContainerFeatures();

  inline void SetEngineWrapperInstancePtr(void *instancePtr) {
    _engineWrapperInstancePtr = instancePtr;
  }

  inline void *GetEngineWrapperInstancePtr() const {
    return _engineWrapperInstancePtr;
  }

  void ScheduleAuthorityTransfer(const std::string &entityUUID,
                                 const std::string &prevOwnerGrapeId,
                                 const std::string &prevOwnerContainerId,
                                 const std::string &newOwnerChunkId);

  void InstantiateEntityLocally(const std::string &entityId,
                                const std::string &informationToLoad,
                                const std::string &props);

private:
#ifdef CELTE_SERVER_MODE_ENABLED
  std::string __rp_fetchContainerFeatures();
  bool __rp_remoteTakeEntity(const std::string &entityId,
                             const std::string &callerId,
                             const std::string &prevOwnerContainerId);

  void __containerTakes(const std::string &containerName,
                        const std::string &transferInfo,
                        const std::string &informationToLoad,
                        const std::string &props, int tick);

  void __containerDrops(const std::string &containerName,
                        const std::string &transferInfo, int tick);
#endif

  void __updateRemoteSubscriptions();

  void __initNetwork();
  /**
   * Subdivide the grape bounding box into options.subdivision chunks.
   */
  void __subdivide();

  std::shared_ptr<IEntityContainer>
  __defaultInstantiateContainer(const std::string &containerId);

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief This RPC will be called when a player requests to spawn in the
   * game. It will instantiate the player in all peers listening to the chunk
   * the player is spawning in by calling a __rp_spawnPlayer RPC to the
   * chunk's rpc channel
   */
  bool __rp_onSpawnRequested(std::string &clientId, std::string &payload);

  void __spawnEntityOnNetwork(const std::string &entityId,
                              const std::string &containerId,
                              const std::string &payload, float x, float y,
                              float z);

  void __ownerExecEntitySpawnProcess(const std::string &entityId,
                                     const std::string &payload, float x,
                                     float y, float z);
#endif

  void __execEntitySpawnProcess(const std::string &entityId,
                                const std::string &containerId,
                                const std::string &payload, float x, float y,
                                float z);

  void __spawnEntityLocally(const std::string &entityId,
                            const std::string &payload, glm::vec3 position,
                            std::function<void()> then);

  void __waitEntityReady(const std::string &entityId,
                         std::function<void()> then);

  void __callSpawnHook(const std::string &entityId, const std::string &payload,
                       glm::vec3 position);

  void __rp_scheduleEntityAuthorityTransfer( // deprecated
      const std::string &entityUUID, const std::string &newOwnerChunkId,
      const std::string &newOwnerGrapeId, const std::string &informationToLoad,
      int tick);

  std::optional<net::RPCService> _rpcs = std::nullopt;

  std::function<glm::vec3(const std::string &)> _entityPositionGetter = nullptr;

  ReplicationGraph _rg;

  const GrapeOptions _options;
  std::unordered_map<std::string, std::shared_ptr<Chunk>> _chunks;

  void *_engineWrapperInstancePtr = nullptr;

  std::chrono::time_point<std::chrono::system_clock> _lastRemoteSubUpdate;
};
} // namespace chunks
} // namespace celte