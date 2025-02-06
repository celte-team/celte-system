#pragma once

#include "Container.hpp"
#include "tbb/concurrent_set.h"
#include <functional>
#include <optional>

namespace celte {
/// @brief This class is used to manage subscription to containers that are not
/// owned locally.
class ContainerSubscriptionComponent {
public:
  /// @brief Subscribes this peer to the container with the given id. Does not
  /// take ownership, simply subscribes to the container.
  /// If this peer is already subscribed to the container, the onReady callback
  /// will be called immediately and the ref count of the container will be
  /// increased.
  /// @param containerId if left empty, the container will have a random id.
  /// @param onReady
  /// @param isLocallyOwned using this parameter in client mode will have no
  /// effect.
  /// @return The id of the container that was subscribed to, or std::nullopt if
  /// this container sub component was already subscribed to the container.
  std::optional<std::string> Subscribe(const std::string &containerId,
                                       std::function<void()> onReady,
                                       bool isLocallyOwned = false);

  /// @brief Unsubscribes this peer from the container with the given id. If
  /// there are no othe references to this container, it will be removed from
  /// the container registry and this peer will no longer be able to access it.
  /// If there are still references to the container, the ref count of the
  /// container
  /// will be decremented.
  void Unsubscribe(const std::string &containerId);

  inline bool IsSubscribed(const std::string &containerId) {
    std::lock_guard<std::mutex> lock(_subscriptionMutex);
    return _subscriptions.find(containerId) != _subscriptions.end();
  }

private:
  std::set<std::string> _subscriptions;
  std::mutex _subscriptionMutex;
};
} // namespace celte