#pragma once
#include "CelteChunk.hpp"
#include "Replicator.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace celte {
/**
 * @brief This class is the base class of all entities managed by celte.
 * Engine integrations should include this class as either the base class
 * of their entities or wrap it as a component.
 *
 * It manages the entity's lifecycle as seen by Celte.
 */
class CelteEntity : public std::enable_shared_from_this<CelteEntity> {
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
  void OnChunkTakeAuthority(celte::chunks::Chunk &chunkId);

  /**
   * @brief Returns the uuid of the entity.
   */
  inline const std::string &GetUUID() const { return _uuid; }

  /**
   * @brief This method is called as often as possible to udpate the
   * internal logic of the entity.
   */
  void Tick();

  /**
   * @brief Returns the chunk that owns this entity.
   * @exception std::out_of_range if the entity is not owned by any chunk.
   */
  inline celte::chunks::Chunk &GetOwnerChunk() const {
    if (_ownerChunk == nullptr) {
      throw std::out_of_range("Entity is not owned by any chunk.");
    }
    return *_ownerChunk;
  }

  inline void SetUUID(const std::string &uuid) { _uuid = uuid; }

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Calling the method will replicate the properties of this entity
   * to the chunk channels of the chunk that owns this entity.
   */
  void UploadReplicationData();

#endif

  std::string GetProps();

  void RegisterReplicatedValue(const std::string &name,
                               std::function<std::string()> get,
                               std::function<void(std::string)> set);

  /**
   * @brief Sets the information that should be used to load this entity by
   * clients or other server nodes.
   * This string should be set by the developer when the entity is first
   * instantiated, and should contain enough information for the dev to load
   * the entity on the client side.
   */
  void SetInformationToLoad(const std::string &info);

  /**
   * @brief Returns the information that should be used to load this entity by
   * clients or other server nodes.
   */
  const std::string &GetInformationToLoad() const;

  /**
   * @brief Returns true if the OnSpawn method has been called without errort
   * and the entity is active in the game.
   */
  inline bool IsSpawned() const { return _isSpawned; }

  /**
   * @brief This method is called when the entity receives data from the network
   * and should update it in order to rollback to the changes made by the
   * server that has authority over the entity.
   */
  void DownloadReplicationData(const std::string &blob);

  /**
   * @brief Send an input to kafka, this will trigger a RPC in the other client
   * and server. Define in CelteInputSystem
   *
   * @param inputName String name/id of the input
   * @param pressed   Bool   status of the input (true for down false for up)
   */
  void sendInputToKafka(std::string inputName, bool pressed);

  /**
   * @brief Returns true if the entity is owned by the current peer.
   */
  bool IsOwnedByCurrentPeer() const;

private:
  std::string _uuid;
  celte::chunks::Chunk *_ownerChunk = nullptr;
  bool _isSpawned = false;

  /**
   * @brief In server mode, this object is used to collect
   * the data to be replicated so that it can later be sent to kafka.
   * On the client side, this is used to overwrite data received from
   * the network.
   */
  runtime::Replicator _replicator;

  // If a peer needs to spawn this entity, the server will send this
  // information which should have been set by the developer when first
  // instantiating the entity, using the SetInformationToLoad on server side.
  // This is a string that can be used to load the entity.
  std::string _informationToLoad;
};
} // namespace celte
