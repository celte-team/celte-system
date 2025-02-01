#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "Container.hpp"
#include "ContainerSubscriptionComponent.hpp"
#include "Executor.hpp"
#include "RPCService.hpp"
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tbb/concurrent_set.h>

namespace celte {

/// @brief Represents a command that must be executed in the engine, but doesn't
/// have an engine agnostic binding.
using InternalCommand = std::string;

struct Grape {
  std::string id;      ///< The unique identifier of the grape.
  bool isLocallyOwned; ///< True if this grape is owned by this peer. (only
                       ///< possible in server mode)
#ifdef CELTE_SERVER_MODE_ENABLED
  std::optional<ClientRegistry> clientRegistry; ///< The client registry for
                                                ///< this grape. Only available
                                                ///< in server mode.
#endif
  Executor executor; ///< The executor for this grape. Tasks to be ran in the
                     ///< engine can be pushed here.
  std::optional<net::RPCService>
      rpcService; ///< The rpc service for this grape. Must be initialized after
  ///< the id has been set.

#ifdef CELTE_SERVER_MODE_ENABLED
  ContainerSubscriptionComponent
      containerSubscriptionComponent; ///< only grapes instances in servers can
                                      ///< sub to a container. In client mode,
                                      ///< this goes through the peer service
  std::set<std::string> ownedContainers; ///< The containers that are owned by
  ///< this grape.
  std::mutex ownedContainersMutex;

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
  subscribeToContainer(const std::string &containerId,
                       std::function<void()> onReady,
                       bool isLocallyOwned = false);

  /// @brief Unsubscribes this peer from a container owned by the grape passed
  /// in argument. if the grape is owned by this peer, does nothing.
  void unsubscribeFromContainer(const std::string &containerId);
  void fetchExistingContainers();
#endif

  std::set<std::string> _proxySubscriptions; ///< used for grapes that are not
                                             ///< locally owned to
  ///< keep track of which locally owned containers
  ///< they are subscribed to.

  void initRPCService();

private:
#ifdef CELTE_SERVER_MODE_ENABLED
  std::vector<std::string> __rp_getExistingOwnedContainers();
#endif
};

} // namespace celte