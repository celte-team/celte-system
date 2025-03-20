#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "CRPC.hpp"
#include "Container.hpp"
#include "ContainerSubscriptionComponent.hpp"
#include "Executor.hpp"

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tbb/concurrent_set.h>
namespace celte {
// Wraps a handle to the object representing the container in the engine.
using ContainerNativeHandle = void *;

struct Grape {
  std::string id;      ///< The unique identifier of the grape.
  bool isLocallyOwned; ///< True if this grape is owned by this peer. (only
                       ///< possible in server mode)
                       // #ifdef CELTE_SERVER_MODE_ENABLED
  Executor executor;   ///< The executor for this grape. Tasks to be ran in the
                       ///< engine can be pushed here.

#ifdef CELTE_SERVER_MODE_ENABLED
  ContainerSubscriptionComponent
      containerSubscriptionComponent; ///< only grapes instances in servers can
                                      ///< sub to a container. In client mode,
                                      ///< this goes through the peer service
  std::map<std::string, std::optional<ContainerNativeHandle>>
      ownedContainers; ///< The containers that are
                       ///< owned by this grape.
  std::mutex ownedContainersMutex;

  inline std::optional<ContainerNativeHandle>
  getOwnedContainerNativeHandle(const std::string &containerId) {
    std::lock_guard<std::mutex> lock(ownedContainersMutex);
    auto it = ownedContainers.find(containerId);
    if (it != ownedContainers.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  inline void setOwnedContainerNativeHandle(const std::string &containerId,
                                            ContainerNativeHandle handle) {
    std::lock_guard<std::mutex> lock(ownedContainersMutex);
    ownedContainers[containerId] = handle;
  }

  /// @brief Subscribes this peer to a container owned by the grape passe in
  /// argument. if the grape is owned by this peer, does nothing.
  /// @param containerId if left empty, the container will have a random id if
  /// it already exists.
  /// @param onReady a callback that will be called when the container is ready.
  /// @param isLocallyOwned if true, the container will be considered locally
  /// owned.
  /// @return The id of the container, or std::nullopt if the container was
  /// already subscribed to by this particular grape.
  std::optional<std::string>
  __subscribeToContainer(const std::string &containerId,
                         std::function<void()> onReady,
                         bool isLocallyOwned = false);

  /// @brief Unsubscribes this peer from a container owned by the grape passed
  /// in argument. if the grape is owned by this peer, does nothing.
  void __unsubscribeFromContainer(const std::string &containerId);

  void fetchExistingContainers();
#endif

  /// @brief Pushes an named task to be executed in the engine's context, with
  /// the arguments given to this method.
  /// @param name
  /// @param args
  template <typename... Args>
  void pushNamedTaskToEngine(const std::string &name, Args... args) {
    decltype(_namedTasksQueues)::accessor acc;
    if (_namedTasksQueues.find(acc, name)) {
      acc->second.push(
          std::make_any<std::tuple<Args...>>(std::make_tuple(args...)));
    } else {
      std::queue<std::any> q;
      q.push(std::make_any<std::tuple<Args...>>(std::make_tuple(args...)));
      _namedTasksQueues.insert(acc, name);
      acc->second = std::move(q);
    }
  }

  /// @brief Pops a named task from the engine's context from its name and
  /// returns the arguments passed to it. This method is called in the engine by
  /// the instance of this grape, through the celte api.
  /// @param name
  /// @return std::optional<std::tuple<Args...>>
  template <typename... Args>
  std::optional<std::tuple<Args...>>
  popNamedTaskFromEngine(const std::string &name) {
    decltype(_namedTasksQueues)::accessor acc;
    if (_namedTasksQueues.find(acc, name)) {
      if (acc->second.empty()) {
        return std::nullopt;
      }
      auto any = acc->second.front();
      acc->second.pop();
      return std::any_cast<std::tuple<Args...>>(any);
    }
    return std::nullopt;
  }

  std::set<std::string> _proxySubscriptions; ///< used for grapes that are not
                                             ///< locally owned to
  ///< keep track of which locally owned containers
  ///< they are subscribed to.

  tbb::concurrent_hash_map<std::string, std::queue<std::any>> _namedTasksQueues;

  void initRPCService();

/* ---------------------------------- RPCS ----------------------------------
 */
#ifdef CELTE_SERVER_MODE_ENABLED
  std::map<std::string, std::string>
  GetExistingEntities(std::string containerId);
  std::vector<std::string> GetExistingOwnedContainers();
  bool SubscribeToContainer(std::string ownerOfContainerId,
                            std::string containerId);
  bool UnsubscribeFromContainer(std::string ownerOfContainerId,
                                std::string containerId);
  bool ProxyTakeAuthority(std::string entityId, std::string fromContainerId,
                          std::string payload);
  bool RequestClientDisconnect(std::string clientId);
  bool Ping();
#endif
  bool ExecClientDisconnect(std::string clientId, std::string payload);

private:
  /// @brief Called when the client with the given id is disconnected from the
  /// cluster. It will clear all the data relatie to this client from the
  /// systems. Engine cleanup should be performed by the game developer, in the
  /// onClientDisconnect hook.
  void __cleanupClientData(const std::string &clientId);
};

REGISTER_SERVER_RPC(Grape, GetExistingEntities);
REGISTER_SERVER_RPC(Grape, GetExistingOwnedContainers);
REGISTER_SERVER_RPC(Grape, SubscribeToContainer);
REGISTER_SERVER_RPC(Grape, UnsubscribeFromContainer);
REGISTER_SERVER_RPC(Grape, ProxyTakeAuthority);
REGISTER_SERVER_RPC(Grape, RequestClientDisconnect);
REGISTER_SERVER_RPC(Grape, Ping);
REGISTER_RPC(Grape, ExecClientDisconnect);

} // namespace celte