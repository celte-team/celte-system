#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "ContainerSubscriptionComponent.hpp"
#include "CustomRPC.hpp"
#include "ETTRegistry.hpp"
#include "GlobalRPC.hpp"
#include "WriterStreamPool.hpp"
#include <functional>
#include <map>

using namespace std::chrono_literals;

namespace celte {
    /// @brief The PeerService class is responsible for managing the connection of
    /// this instance of the celte runtime to the cluster.
    /// It holds the RPC endpoints for other peers to run rpcs on this instance.
    class PeerService : public CustomRPCTemplate {
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

        inline Global& GetGlobalRPC() { return _globalRPC; }

#ifdef CELTE_SERVER_MODE_ENABLED
        void ConnectClientToThisNode(const std::string& clientId,
            std::function<void()> then);

        void SubscribeClientToContainer(const std::string& clientId,
            const std::string& containerId,
            std::function<void()> then);

        void UnsubscribeClientFromContainer(const std::string& clientId,
            const std::string& containerId);

        inline ClientRegistry& GetClientRegistry() { return _clientRegistry; }

#endif

        void RPCHandler(std::string RPCname, std::string args)
        {
            Handler(RPCname, args, tp::peer(RUNTIME.GetUUID()));
        }

        /// @brief If the peer is a client returns a map of the latencies (ms) to each
        /// of the known server nodes in the cluster.
        /// @return
        std::map<std::string, int> GetLatency();

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
    public:
        /// @brief Forces the client to connect to a specific node. Will register
        /// the associated grape in the grape registry.
        bool ForceConnectToNode(std::string grapeId);

        /// @brief Subscribes the client to a container's network services.
        bool SubscribeClientToContainer(std::string containerId,
            std::string ownerGrapeId);
        /// @brief RPC called by servers interested in this client to check if it is
        /// still alive.
        /// @note the bool argument is not used, it is just a placeholder to make the
        /// rpc call.

        bool UnsubscribeClientFromContainer(std::string containerId);

    private:
#endif

    public:
        bool Ping();

    private:
        /* ------------------------------- SERVER RPC
         * -------------------------------
         */
#ifdef CELTE_SERVER_MODE_ENABLED
    public:
        /// @brief  Sets this server node as the owner of this grape.
        /// This node is expected to load the grape in game.
        /// @param grapeId
        /// @return
        bool AssignGrape(std::string grapeId);

        /// @brief Called by the master server, this method should return the name
        /// of the grape that the client should connect to.
        std::string RequestSpawnPosition(std::string clientId);

        /// @brief Called by the master server, this method notifies a server node
        /// that a client has been assigned to it.
        bool AcceptNewClient(std::string clientId);

    private:
        ClientRegistry _clientRegistry;
#else

        ContainerSubscriptionComponent _containerSubscriptionComponent;
#endif

        net::WriterStreamPool _wspool;
        Global _globalRPC;
    };

    REGISTER_SERVER_RPC(PeerService, AssignGrape);
    REGISTER_SERVER_RPC(PeerService, RequestSpawnPosition);
    REGISTER_SERVER_RPC(PeerService, AcceptNewClient);

    REGISTER_CLIENT_RPC(PeerService, ForceConnectToNode);
    REGISTER_CLIENT_RPC(PeerService, SubscribeClientToContainer);
    REGISTER_CLIENT_RPC(PeerService, UnsubscribeClientFromContainer);

    REGISTER_RPC(PeerService, Ping);
    REGISTER_RPC(PeerService, RPCHandler);

} // namespace celte
