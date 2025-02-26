#include "ContainerSubscriptionComponent.hpp"

using namespace celte;

std::optional<std::string>
ContainerSubscriptionComponent::Subscribe(const std::string &containerId,
                                          std::function<void()> onReady,
                                          bool isLocallyOwned) {
  LOGINFO("Subscribing to container " + containerId);
  std::cout << "SUBSCRIBING TO CONTAINER " << containerId << std::endl;
  if (_subscriptions.find(containerId) != _subscriptions.end()) {
    onReady();
    return std::nullopt;
  }

  bool wasCreated = false;
  std::string id = ContainerRegistry::GetInstance().CreateContainerIfNotExists(
      containerId, &wasCreated);
  {
    std::lock_guard<std::mutex> lock(_subscriptionMutex);
    _subscriptions.insert(id);
  }
  if (wasCreated) {
    ContainerRegistry::GetInstance().RunWithLock(
        id, [onReady, &id, &containerId,
             isLocallyOwned](ContainerRegistry::ContainerRefCell &cell) {
          cell.IncRefCount();
          auto &container = cell.GetContainer();
#ifdef CELTE_SERVER_MODE_ENABLED
          if (isLocallyOwned) {
            container._isLocallyOwned = true;
          } else {
#endif
            container._isLocallyOwned = false;
#ifdef CELTE_SERVER_MODE_ENABLED
          }
#endif
          container.__initRPCs();
          container.__initStreams();
          container.WaitForNetworkReady(onReady);
        });
  }
  return id;
}

void ContainerSubscriptionComponent::Unsubscribe(
    const std::string &containerId) {
  LOGINFO("Unsubscribing from container " + containerId);
  if (_subscriptions.find(containerId) == _subscriptions.end()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(_subscriptionMutex);
    _subscriptions.erase(containerId);
  }
  ContainerRegistry::GetInstance().RunWithLock(
      containerId,
      [](ContainerRegistry::ContainerRefCell &cell) { cell.DecRefCount(); });
  ContainerRegistry::GetInstance().UpdateRefCount(containerId);
}
