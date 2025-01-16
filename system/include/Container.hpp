/*
** CELTE, 2025
** refacto

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** Container
*/

#pragma once
#include "CelteService.hpp"
#include "RPCService.hpp"
#include "Replicator.hpp"
#include "systems_structs.pb.h"
#include <memory>
#include <mutex>
#include <string>
#include <tbb/concurrent_queue.h>

namespace celte {
    class Container : public net::CelteService {
    public:
        Container();

        /// @brief Attaches this container to  a grape.
        /// This will make the container part of the grape's network and entities that
        /// belong to the grape will start being assigned to this container.
        /// @warning This method will attempt to lock the grape registry for this
        /// grape, be careful of deadlocks.
        /// @param grapeId
        /// @return true if the container was successfully attached to the grape.
        bool AttachToGrape(const std::string& grapeId);

        /// @brief Waits until the network of this container is ready, then calls the
        /// provided callback.
        /// @param onReady
        void WaitForNetworkReady(std::function<void(bool)> onReady);

        inline const std::string& GetId() const { return _id; }

        inline const std::string& GetGrapeId() const { return _grapeId; }

        inline bool IsLocallyOwned() const { return _isLocallyOwned; }

#ifdef CELTE_SERVER_MODE_ENABLED

        void PushReplToQueue(Replicator::ReplBlob repl, std::string id);
        void _sendQueueToRepl(bool ready);
#endif

    private:
        void __initRPCs();
        void __initStreams();
        void __handlerReplMessage(req::ReplicationDataPacket req);

        void __rp_containerTakeAuthority(const std::string& args);
        void __rp_containerDropAuthority(const std::string& args);

        std::string _id; ///< Unique id on the network
        std::string _grapeId; ///< The grape this container belongs to
        bool _isLocallyOwned; ///< True if this container is owned by this peer
        net::RPCService
            _rpcService; ///< The rpc service for this container, for calling methods
        ///< on all peers listening to this container.

#ifdef CELTE_SERVER_MODE_ENABLED
        std::shared_ptr<net::WriterStream>
            _replws; ///< The writer stream for replication
        std::shared_ptr<tbb::concurrent_queue<std::pair<std::string, Replicator::ReplBlob>>> _replToPush;
#endif
    };
} // namespace celte
