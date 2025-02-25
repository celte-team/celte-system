#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "ContainerSubscriptionComponent.hpp"
#include "RPCService.hpp"
#include "WriterStreamPool.hpp"
#include <functional>

using namespace std::chrono_literals;

namespace celte {
/// @brief The PeerService class is responsible for managing the connection of
/// this instance of the celte runtime to the cluster.
/// It holds the RPC endpoints for other peers to run rpcs on this instance.
class PeerService {
public:
  /// @brief Construct a new Peer Service object
  /// @param onReady A function to be called when the service is ready. The
  /// function will be called with a boolean indicating if the service is
  /// successfully connected to the cluster.
  /// @param connectionTimeout The time to wait for the service to be ready
  /// before failing the connection.
  PeerService(std::function<void(bool)> onReady,
              std::chrono::milliseconds connectionTimeout = 500ms);

  /// @brief Destroy the Peer Service object
  ~PeerService();

  /// @brief Returns the RPCService of this peer.
  inline net::RPCService &GetRPCService() { return *_rpcService; }

#ifdef CELTE_SERVER_MODE_ENABLED
  void ConnectClientToThisNode(const std::string &clientId,
                               std::function<void()> then);

  void SubscribeClientToContainer(const std::string &clientId,
                                  const std::string &containerId,
                                  std::function<void()> then);

  void UnsubscribeClientFromContainer(const std::string &clientId,
                                      const std::string &containerId);

  inline ClientRegistry &GetClientRegistry() { return _clientRegistry; }

#endif

private:
  /// @brief Waits for the network of the rpc service to be ready
  /// @param connectionTimeout The time to wait for the network to be ready
  /// @return true if the network is ready, false otherwise
  bool __waitNetworkReady(std::chrono::milliseconds connectionTimeout);

  /// @brief Initializes the rpc endpoints for the peer service (registers
  /// methods specific to this peer)
  void __initPeerRPCs();

  /// @brief Pings the master server to let it know that this peer is ready
  void __pingMaster(std::function<void(bool)> onReady);

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Registers the rpc endpoints for the server mode
  void __registerServerRPCs();
#else
  /// @brief Registers the rpc endpoints for the client mode
  void __registerClientRPCs();
#endif

  /* ------------------------------- CLIENT RPC
   * -------------------------------
   */
#ifndef CELTE_SERVER_MODE_ENABLED // ! ndef, we are in client mode here
  /// @brief Forces the client to connect to a specific node. Will register
  /// the associated grape in the grape registry.
  bool __rp_forceConnectToNode(const std::string &grapeId);

  /// @brief Subscribes the client to a container's network services.
  bool __rp_subscribeClientToContainer(const std::string &containerId,
                                       const std::string &ownerGrapeId);
  /// @brief RPC called by servers interested in this client to check if it is
  /// still alive.
  /// @note the bool argument is not used, it is just a placeholder to make the
  /// rpc call.

  bool __rp_unsubscribeClientFromContainer(const std::string &containerId);
  bool __rp_ping();
#endif
  /* ------------------------------- SERVER RPC
   * -------------------------------
   */
#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief  Sets this server node as the owner of this grape.
  /// This node is expected to load the grape in game.
  /// @param grapeId
  /// @return
  bool __rp_assignGrape(const std::string &grapeId);

  /// @brief Called by the master server, this method should return the name
  /// of the grape that the client should connect to.
  std::string __rp_spawnPositionRequest(const std::string &clientId);

  /// @brief Called by the master server, this method notifies a server node
  /// that a client has been assigned to it.
  bool __rp_acceptNewClient(const std::string &clientId);

  ClientRegistry _clientRegistry;
#else

  ContainerSubscriptionComponent _containerSubscriptionComponent;
#endif

  net::WriterStreamPool _wspool;
  std::optional<net::RPCService> _rpcService;
};
} // namespace celte