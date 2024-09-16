#pragma once
#include "CelteEntity.hpp"
#include "RotatedBoundingBox.hpp"
#include <glm/vec3.hpp>
#include <string>

namespace celte {
namespace chunks {
struct ChunkConfig {
  const std::string chunkId;
  const std::string grapeId;
  const glm::vec3 position;
  const glm::vec3 localX;
  const glm::vec3 localY;
  const glm::vec3 localZ;
  const glm::vec3 size;
};

/**
 * @brief A chunk is a region of the world which is under a unique server node's
 * control. All entities or the same chunk are replicated together: a chunk is
 * the smallest container of entities that can be moved from one server node to
 * another.
 *
 * Chunks handle entity replication and authority over entities.
 */
class Chunk {
public:
  Chunk(const ChunkConfig &config);
  ~Chunk();

  /**
   * @brief Called when an entity enters the chunk.
   */
  void OnEntityEnter(CelteEntity &celteEntity);

  /**
   * @brief Called when an entity exits the chunk.
   */
  void OnEntityExit(CelteEntity &celteEntity);

  /**
   * @brief Called when an entity spawns in the chunk.
   */
  void OnEntitySpawn(CelteEntity &celteEntity);

  /**
   * @brief Called when an entity despawns in the chunk.
   */
  void OnEntityDespawn(CelteEntity &celteEntity);

  /**
   * @brief Returns true if the given position is inside the chunk.
   */
  bool ContainsPosition(float x, float y, float z) const;

  inline const std::string &GetChunkId() const { return _config.chunkId; }

  inline const std::string &GetGrapeId() const { return _config.grapeId; }

  inline const std::string &GetCombinedId() const { return _combinedId; }

  /**
   * @brief Takes authority of an entity by its id.
   * The provided id must be registered in with CelteEntity.
   * If that is not the case, this action will be buffered until an entity
   * with this id is registered in the game.
   *
   */
  void TakeAuthority(const std::string &entityId);

private:
  /**
   * @brief Registers all consumers for the chunk.
   * The consumers listen for events in the chunk's topic and react to them.
   */
  void __registerConsumers();

  RotatedBoundingBox _boundingBox;

  const ChunkConfig _config;
  const std::string _combinedId;
};
} // namespace chunks
} // namespace celte