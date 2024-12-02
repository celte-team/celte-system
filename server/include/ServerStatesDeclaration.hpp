#pragma once
#include "CelteServer.hpp"
#include "ServerEvents.hpp"
#include "ServerNetService.hpp"
#include "tinyfsm.hpp"
#include <set>

namespace celte {
namespace server {
namespace states {
ServerNetService &ServerNet();

/**
 * This is the default state for the server, before AKafkaLink
 * connects to kafka. It should initialize the server and wait for
 * the connection to be established.
 */
class Disconnected : public AServer {
  void entry() override;
  void exit() override;

  void react(EConnectToCluster const &event) override;
};

/**
 * When the EConnectToCluster event is received, the server should
 * start connecting to the server. This state waits until kafka is
 * connected and queries the server for its UUID. (TODO)
 */
class Connecting : public AServer {
  void entry() override;
  void exit() override;

  /**
   * When the connection to the server succeeds, the server will
   * transit to the Connected state.
   */
  void react(EConnectionSuccess const &event) override;

  // /**
  //  * @brief This method will be called when the server receives a UUID
  //  * from the cluster. It will set the UUID in the runtime and send a
  //  * message to the master node to signal that the server is ready.
  //  */

  // void __onUUIDReceived(const kafka::clients::consumer::ConsumerRecord&
  // record);

  // /**
  //  * @brief This method will be called when the server sends a message to
  //  * the master node to signal that the server is ready.
  //  */

  // void __onHelloDelivered(const kafka::clients::producer::RecordMetadata&
  // metadata,
  //     kafka::Error error);
};

/**
 * When the connection to the server is established, this state will
 * handle the gameplay logic for the server.
 */
class Connected : public AServer {
  void entry() override;
  void exit() override;

  void react(EDisconnectFromServer const &event) override;

  /**
   * @brief This method will registered the RPCs required for the server to
   * function normally when is is connected to the cluster and ready to manage
   * the game loop.
   *
   * This is called by celte::server::states::Connected::entry.
   */
  void __registerRPCs();

  /**
   * @brief This method will unregister the RPCs registered by the
   * __registerRPCs method.
   *
   * This is called by celte::server::states::Connected::exit.
   */
  void __unregisterRPCs();

  /* --------------------------------------------------------------------------
   */
  /*                                    RPCs */
  /* --------------------------------------------------------------------------
   */

  /**
   * @brief This RPC will be called when a new player connects to the server.
   * It will be called by the client when it connects to the server.
   *
   * # Hooks:
   * This RPC refers to the following hooks:
   * - celte::api::HooksTable::server::newPlayerConnected::accept
   *
   *
   * @param clientId The UUID of the client that connected to the server.
   * @param grapeId The UUID of the grape that the client is spawning in.
   * @param x The x coordinate where the player should spawn.
   * @param y The y coordinate where the player should spawn.
   * @param z The z coordinate where the player should spawn.
   */
  void __rp_acceptNewClient(std::string clientId, std::string grapeId, float x,
                            float y, float z);

  /**
   * @brief This RPC will be called by clients when they want to spawn their
   * player in the game world.
   *
   * # Hooks:
   * This RPC refers to the following hooks:
   * - celte::api::HooksTable::server::newPlayerConnected::spawnPlayer
   *
   * @param clientId The UUID of the client that connected to the server.
   * @param x The x coordinate where the player should spawn.
   * @param y The y coordinate where the player should spawn.
   * @param z The z coordinate where the player should spawn.
   */
  void __rp_spawnPlayer(std::string clientId, float x, float y, float z);

  /**
   * @brief This RPC will be called when a player leaves the area of authority
   * of this node.
   *
   */
  void __rp_dropPlayerAuhority(std::string clientId);

  /**
   * @brief This RPC will be called when a player requests to spawn in the game.
   * It will instantiate the player in all peers listening to the chunk the
   * player is spawning in by calling a __rp_spawnPlayer RPC to the chunk's rpc
   * channel
   */
  void __rp_onSpawnRequested(const std::string &clientId, float x, float y,
                             float z);

  /**
   * @brief The master can call on to this method to assign a grape to this
   * node. No other node should be actively listening to this grape's private
   * channels.
   */
  void __rp_assignGrape(std::string grapeId);

  /**
   * @brief Registers the basic consumers of this node's grape.
   * This method is called by __rp_assignGrape.
   */
  void __registerGrapeConsumers(const std::string &grapeId);

  /**
   * @brief Unregisters the basic consumers of this node's grape.
   * This method is called when the server transits to the Disconnected state.
   */
  void __unregisterGrapeConsumers();

  /**
   * @brief This method is called by the server to ask the server node to find
   * out where the player should be spawning in the map.
   */
  std::tuple<std::string, std::string, float, float, float>
  __rp_getPlayerSpawnPosition(const std::string &clientInfo);

  // /**
  //  * @brief This rp is called by the client when it is ready to spawn,
  //  connected
  //  * to the grape and ready to play. The client has to be connected to chunk
  //  * topics before calling this rp to ensure that no data created after the
  //  * execution of this procedure is lost.
  //  */
  // void __rp_sendExistingEntitiesSummary(std::string clientId,
  //                                       std::string grapeId);

  void __rp_loadExistingEntities(std::string clientId, std::string summary);

  /**
   * @brief stores the clients that are under this node's authority.
   * Clients are added using the __rp_acceptNewClient RPC and removed
   * using the __rp_disconnectPlayer RPC. Disconnection happends when they
   * leave this node's chunk grape.
   */
  std::set<std::string> m_clients;

  // the id of the assigned grape. Invalid if empty.
  std::string _grapeId = "";
};
} // namespace states
} // namespace server
} // namespace celte
