#pragma once
#include "CelteChunk.hpp"
#include "Replicator.hpp"
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
   * @param uuid: The uuid of the entity. If empty, a random uuid will be
   * generated. The uuid will typically be empty if the uuid is owned by the
   * current peer, and filled if the entity is owned by another peer (and the
   * spawn order is sent through the network).
   */
  void OnSpawn(float x, float y, float z, const std::string &uuid = "");

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

  /**
   * @brief This method is called as often as possible to udpate the
   * internal logic of the entity.
   */
  void Tick();

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Calling the method will replicate the properties of this entity
   * to the chunk channels of the chunk that owns this entity.
   */
  void UploadReplicationData();

  /**
   * @brief Resets the data changed flag for all data.
   */
  inline void ResetDataChanged() { _replicator.ResetDataChanged(); }
#endif

private:
  std::string _uuid;
  celte::chunks::Chunk *_ownerChunk = nullptr;
  runtime::Replicator _replicator;
};
} // namespace celte