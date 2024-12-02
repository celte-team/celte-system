#pragma once
#include "CelteClient.hpp"
#include "ClientEvents.hpp"
#include "ClientNetService.hpp"
#include "tinyfsm.hpp"

namespace celte {
namespace client {
namespace states {
ClientNetService &ClientNet();

/**
 * This is the default state for the client, before AKafkaLink
 * connects to kafka. It should initialize the client and wait for
 * the connection to be established.
 */
class Disconnected : public AClient {
  void entry() override;
  void exit() override;

  /**
   * When the EConnectToCluster event is received, the client
   * should start connecting to the server. It will transit to the
   * Connecting state.
   */
  void react(EConnectToCluster const &event) override;
};

/**
 * When the EConnectToCluster event is received, the client should
 * start connecting to the server. This state waits until kafka is
 * connected and queries the server for its UUID. (TODO)
 */
class Connecting : public AClient {
  void entry() override;
  void exit() override;

  /**
   * @brief When the connection to the server succeeds, the client will
   * transit to the Connected state.
   */
  void react(EConnectionSuccess const &event) override;

  /**
   * @brief Subscribes to the basic topics required for the client to
   * operate normally.
   */
  void __subscribeToTopics();
};

/**
 * When the connection to the server is established, this state will
 * handle the gameplay logic for the client.
 */
class Connected : public AClient {
  void entry() override;
  void exit() override;

  /*
   * When the server disconnects, the client will transit to the
   * Disconnected state.
   */
  void react(EDisconnectFromServer const &event) override;

  /**
   * @brief This method will registered the RPCs required for the client to
   * function normally when is is connected to the cluster and ready to manage
   * the game loop.
   */
  void __registerRPCs();

  /**
   * @brief This method will unregister the RPCs registered by the
   * __registerRPCs method.
   */
  void __unregisterRPCs();

  /* --------------------------------------------------------------------------
   */
  /*                                    RPCs */
  /* --------------------------------------------------------------------------
   */

  /**
   * @brief This RPC is called by the server node managing this client, to
   * assign it to a chunk when it first spawns in the game world.
   */
  void __rp_forceConnectToChunk(std::string grapeId, float x, float y, float z);

  /**
   * @brief This RPC is called by the server when it has spawned the player and
   * the client should do the same.
   *
   * TODO: generalize for handling other entities and other players
   */
  void __rp_spawnPlayer(std::string clientId, float x, float y, float z);

  /**
   * @brief This RPC is called by the server when the client connects to a
   * grape. The arguments contain a list of all entites that are currently
   * loaded in the grape. (see
   * CelteEntityManagementSystem::GetRegisteredEntitiesSummary) This RPC will
   * call a hook that should be used by the game dev to load the entities in the
   * grape, assigning them to the correct chunks.
   * There is no need to set the positions because the entities will be
   * replicated anyway.
   */
  void __rp_loadExistingEntities(std::string grapeId, std::string summary);
};
} // namespace states
} // namespace client
} // namespace celte