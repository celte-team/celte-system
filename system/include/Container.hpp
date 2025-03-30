#pragma once
#include "CelteService.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"

#include "systems_structs.pb.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <tbb/concurrent_hash_map.h>
#ifdef CELTE_SERVER_MODE_ENABLED
#include <set>
#endif
#include "CRPC.hpp"

namespace celte {
    class Grape;
    class ContainerSubscriptionComponent;
    class Container : public net::CelteService {
    public:
        Container();
        ~Container();

        /// @brief Attaches this container to  a grape.
        /// This will make the container part of the grape's network and entities
        /// that belong to the grape will start being assigned to this container.
        /// @warning This method will attempt to lock the grape registry for this
        /// grape, be careful of deadlocks.
        /// @param grapeId
        /// @return true if the container was successfully attached to the grape.
        // bool AttachToGrape(const std::string &grapeId);

        /// @brief Waits until the network of this container is ready, then calls
        /// the provided callback.
        /// @param onReady
        void WaitForNetworkReady(std::function<void()> onReady);

        inline const std::string& GetId() const { return _id; }

        inline const std::string& GetGrapeId() const { return _grapeId; }

        inline bool IsLocallyOwned() const { return _isLocallyOwned; }

#ifdef CELTE_SERVER_MODE_ENABLED
        inline std::shared_ptr<net::WriterStream> GetReplicationWriterStream()
        {
            return _replws;
        }
#endif

        /// @brief Manually set the id of a container. Called when creating a
        /// container on the order of a server node, in
        /// PeerService::SubscribeToContainer
        /// @param id
        inline void __setIdInternal(const std::string& id) { _id = id; }

        void TakeAuthority(std::string args);
        void DropAuthority(std::string args);
        void DeleteEntity(std::string entityId, std::string payload);

    private:
        void __initRPCs();
        void __initStreams();

        std::string _id; ///< Unique id on the network
        std::string _grapeId; ///< The grape this container belongs to
        bool _isLocallyOwned; ///< True if this container is owned by this peer
        // net::RPCService _rpcService; ///< The rpc service for this container, for
        ///< calling methods
        ///< on all peers listening to this container.

#ifdef CELTE_SERVER_MODE_ENABLED
        std::shared_ptr<net::WriterStream>
            _replws; ///< The writer stream for replication
        std::set<std::string> _ownedEntities; ///< Entities that are owned by this
                                              ///< container.
#endif

        friend class Grape;
        friend class ContainerRegistry;
        friend class ContainerSubscriptionComponent;
    };

    REGISTER_RPC(Container, TakeAuthority);
    REGISTER_RPC(Container, DropAuthority);
    REGISTER_RPC(Container, DeleteEntity);

    class ContainerRegistry {
    public:
        /// @brief A container reference cell is a struct that holds a shared pointer
        /// to a container id and a container. The shared pointer is used to keep
        /// track of the number of references to the container. We use the shared
        /// pointer on the id instead of on the container to force using thread safe
        /// accessors to access the container.
        struct ContainerRefCell {
            std::string id;
            std::atomic<int> refCount;

            ContainerRefCell(const std::string& containerId)
                : id(containerId)
                , refCount(1)
                , container(std::make_shared<Container>())
            {
                container->__setIdInternal(containerId);
            }

            /// @brief The container that this cell holds a reference to.
            /// @warning Do not store the result of this method, as the reference
            /// may be invalidated.
            inline Container& GetContainer() { return *container; }

            inline std::shared_ptr<Container> GetContainerPtr() { return container; }

            void IncRefCount() { refCount.fetch_add(1, std::memory_order_relaxed); }

            void DecRefCount() { refCount.fetch_sub(1, std::memory_order_relaxed); }

            int GetRefCount() const { return refCount.load(std::memory_order_relaxed); }

        private:
            std::shared_ptr<Container> container; ///< this is a shared ptr to allow
                                                  ///< copy construction of the refcell
            friend class ContainerRegistry;
        };

    private:
        tbb::concurrent_hash_map<std::string, ContainerRefCell> _containers;
        using accessor = tbb::concurrent_hash_map<std::string, ContainerRefCell>::accessor;

    public:
        static ContainerRegistry& GetInstance();

        /// @brief Runs a function with a lock on the container registry.
        void RunWithLock(const std::string& containerId,
            std::function<void(ContainerRefCell&)> f);

        /// @brief Creates the container with the given id if the container does not
        /// exist. If the id is empty, a random id will be generated. Returns the id
        /// of the container.
        std::string
        CreateContainerIfNotExists(const std::string& id,
            __attribute__((nonnull)) bool* wasCreated);

        /// @brief If the number of references to the container is 1 (meaning the only
        /// reference is
        /// the one in the container registry), the container is removed from the
        /// registry, effectively removing the container from the subscriptions of
        /// this peer.
        void UpdateRefCount(const std::string& containerId);

        bool ContainerExists(const std::string& containerId)
        {
            return _containers.count(containerId) > 0;
        }

        bool ContainerIsLocallyOwned(const std::string& containerId)
        {
            accessor acc;
            if (_containers.find(acc, containerId)) {
                return acc->second.GetContainer().IsLocallyOwned();
            }
            return false;
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        inline void RegisterNewOwnedEntityToContainer(const std::string& containerId,
            const std::string& entityId)
        {
            RunWithLock(containerId, [entityId](ContainerRefCell& c) {
                c.GetContainer()._ownedEntities.insert(entityId);
            });
        }

        inline void RemoveOwnedEntityFromContainer(const std::string& containerId,
            const std::string& entityId)
        {
            RunWithLock(containerId, [entityId](ContainerRefCell& c) {
                c.GetContainer()._ownedEntities.erase(entityId);
            });
        }

        std::vector<Entity::ETTNativeHandle>
        GetOwnedEntitiesNativeHandles(const std::string& containerId)
        {
            std::vector<Entity::ETTNativeHandle> handles;
            RunWithLock(containerId, [&handles](ContainerRefCell& c) {
                for (const auto& eid : c.GetContainer()._ownedEntities) {
                    auto handle = ETTREGISTRY.GetEntityNativeHandle(eid);
                    if (handle.has_value()) {
                        handles.push_back(handle.value());
                    }
                }
            });
            return handles;
        }
#endif
    };
} // namespace celte
