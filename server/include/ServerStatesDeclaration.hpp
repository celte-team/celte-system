#pragma once
#include "CelteServer.hpp"
#include "ServerEvents.hpp"
#include "tinyfsm.hpp"

namespace celte {
namespace server {
namespace states {
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
   * - celte::api::HooksTable::server::newPlayerConnected::spawnPlayer
   *
   *
   * @param clientId The UUID of the client that connected to the server.
   * @param x The x coordinate where the player should spawn.
   * @param y The y coordinate where the player should spawn.
   * @param z The z coordinate where the player should spawn.
   */
  void __rp_acceptNewPlayer(std::string clientId, int x, int y, int z);
};
} // namespace states
} // namespace server
} // namespace celte