#pragma once
#include "RotatedBoundingBox.hpp"
#include "topics.hpp"
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
 * @brief A chunk is a region of the world which is under a unique server
 * node's control. All entities or the same chunk are replicated together: a
 * chunk is the smallest container of entities that can be moved from one
 * server node to another.
 *
 * Chunks handle entity replication and authority over entities.
 */
class Chunk {
public:
  Chunk(const ChunkConfig &config);
  ~Chunk();

  /**
   * @brief Returns true if the given position is inside the chunk.
   */
  bool ContainsPosition(float x, float y, float z) const;

  inline const std::string &GetChunkId() const { return _config.chunkId; }

  inline const std::string &GetGrapeId() const { return _config.grapeId; }

  inline const std::string &GetCombinedId() const { return _combinedId; }

private:
  /**
   * @brief Registers all consumers for the chunk.
   * The consumers listen for events in the chunk's topic and react to them.
   */
  void __registerConsumers();

  /**
   * @brief Registers RPCs for the available actions of the chunk.
   * The RPCs are registered in the global RPC table.
   */
  void __registerRPCs();

  /* --------------------------------------------------------------------------
   */
  /*                                    RPCS */
  /* --------------------------------------------------------------------------
   */

  /**
   * @brief Schedules an entity authority transfer.
   * The entity will be transferred to the new authority at the given global
   * clock tick.
   *
   * @param entityId the id of the entity to transfer
   * @param takeAuthority true if the chunk should take authority, false if it
   * should drop it
   * @param atTick the global clock tick at which the transfer should occur
   */
  void __rp_scheduleEntityAuthorityTransfer(std::string entityId,
                                            bool takeAuthority, int atTick);

  /* --------------------------------------------------------------------------
   */
  /*                                   Members */
  /* --------------------------------------------------------------------------
   */

  RotatedBoundingBox _boundingBox;

  const ChunkConfig _config;
  const std::string _combinedId;
};
} // namespace chunks
} // namespace celte