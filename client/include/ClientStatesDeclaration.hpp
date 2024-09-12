#pragma once
#include "CelteClient.hpp"
#include "ClientEvents.hpp"
#include "tinyfsm.hpp"

namespace celte {
namespace client {
namespace states {
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
   * When the connection to the server succeeds, the client will
   * transit to the Connected state.
   */
  void react(EConnectionSuccess const &event) override;
};

/**
 * When the connection to the server is established, this state will
 * handle the gameplay logic for the client.
 */
class Connected : public AClient {
  void entry() override;
  void exit() override;

  /**
   * When the server disconnects, the client will transit to the
   * Disconnected state.
   */
  void react(EDisconnectFromServer const &event) override;
};
} // namespace states
} // namespace client
} // namespace celte