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
};
} // namespace states
} // namespace server
} // namespace celte