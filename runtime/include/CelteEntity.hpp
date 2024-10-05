#pragma once
#include "CelteChunk.hpp"
#include <string>

namespace celte {
/**
 * @brief This class is the base class of all entities managed by celte.
 * Engine integrations should include this class as either the base class
 * of their entities or wrap it as a component.
 *
 * It manages the entity's lifecycle as seen by Celte.
 */
class CelteEntity {
public:
  /**
   * @brief Called when the entity is spawned.
   * This is called when the entity is created and added to the scene.
   * It will generate a uuid for the entity and forward it to the chunk
   * that the entity is spawning in.
   *
   * @param x, y, z: The position of the entity in the world, necessary to
   * determine the chunk.
   */
  void OnSpawn(float x, float y, float z);

  /**
   * @brief This method is called when an entity is destroyed in the game.
   * It will unregister the entity from all systems and notify all peers of the
   * event.
   */
  void OnDestroy();

  /**
   * @brief When a chunk takes ownership of the entity, this method is called.
   * It will register the chunk as the owner of the entity so that the entity
   * can send its data to it when it changes.
   *
   * The chunk will then send the data to kafka.
   *
   * Entites are active in the sending of their data to avoid chunks having to
   * keep track entites being destroyed, etc...
   */
  void OnChunkTakeAuthority(const celte::chunks::Chunk &chunkId);

  /**
   * @brief Returns the uuid of the entity.
   */
  inline const std::string &GetUUID() const { return _uuid; }

private:
  std::string _uuid;
  celte::chunks::Chunk *_ownerChunk = nullptr;
};
} // namespace celte