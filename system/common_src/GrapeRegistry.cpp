#include "AuthorityTransfer.hpp"
#include "GrapeRegistry.hpp"
#include "PeerService.hpp"
#include "Topics.hpp"
#include <functional>

using namespace celte;

GrapeRegistry &GrapeRegistry::GetInstance() {
  static GrapeRegistry instance;
  return instance;
}

void GrapeRegistry::RegisterGrape(const std::string &grapeId,
                                  bool isLocallyOwned,
                                  std::function<void()> onReady) {
  accessor acc;
  if (not _grapes.insert(acc, grapeId))
    throw std::runtime_error("Grape with id " + grapeId + " already exists.");

#ifdef CELTE_SERVER_MODE_ENABLED
  if (isLocallyOwned and not RUNTIME.GetAssignedGrape().empty()) {
    throw std::runtime_error(
        "Server already has a node assigned as locally owned.");
  }
  if (isLocallyOwned) {
    RUNTIME.SetAssignedGrape(grapeId);
  }
#endif

  acc->second.id = grapeId;
  acc->second.isLocallyOwned = isLocallyOwned;
  acc.release();

  if (onReady) {
    RUNTIME.ScheduleAsyncTask([onReady, grapeId, isLocallyOwned]() {
      // wait for the grape's network service to be ready
      accessor acc2;
      if (GRAPES.GetGrapes().find(acc2, grapeId)) {
        acc2->second.initRPCService();
#ifdef CELTE_SERVER_MODE_ENABLED
        if (not isLocallyOwned) {
          acc2->second.fetchExistingContainers();
        }
#endif
        acc2.release();
        GRAPES.PushTaskToEngine(grapeId, onReady);
      }
    });
  }
}

void GrapeRegistry::UnregisterGrape(const std::string &grapeId) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    _grapes.erase(acc);
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::string GrapeRegistry::ContainerCreateAndAttach(
    std::string grapeId, std::function<void()> onReady, const std::string &id) {
  accessor acc;

  if (_grapes.find(acc, grapeId)) {
    auto finalId = acc->second
                       .__subscribeToContainer(
                           id,
                           [onReady]() {
                             RUNTIME.TopExecutor().PushTaskToEngine(onReady);
                           },
                           acc->second.isLocallyOwned)
                       .value_or(id);
    LOGINFO("Grape " + grapeId + " is creating a new owned container with id " +
            finalId);
    return finalId;
  }
  return "error-bad-grape";
}
#endif

bool GrapeRegistry::ContainerExists(const std::string &containerId) {
  return ContainerRegistry::GetInstance().ContainerExists(containerId);
}

bool GrapeRegistry::GrapeExists(const std::string &grapeId) {
  accessor acc;
  return _grapes.find(acc, grapeId);
}

#ifdef CELTE_SERVER_MODE_ENABLED
void GrapeRegistry::SetRemoteGrapeSubscription(
    const std::string &ownerOfContainerId, const std::string &grapeId,
    const std::string &containerId, bool subscribe) {
  if (grapeId == RUNTIME.GetAssignedGrape()) {
    return;
  }
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    if (subscribe && not acc->second._proxySubscriptions.count(containerId)) {
      acc->second._proxySubscriptions.insert(containerId);
      std::cout << "Grape " << grapeId << " is subscribing to container "
                << containerId << " owned by " << ownerOfContainerId
                << std::endl;
      CallGrapeSubscribeToContainer()
          .on_peer(grapeId)
          .on_fail_log_error()
          .with_timeout(std::chrono::milliseconds(1000))
          .retry(3)
          // .fire_and_forget(ownerOfContainerId, containerId);
          .call<bool>(ownerOfContainerId, containerId);
    } else if (not subscribe and
               acc->second._proxySubscriptions.count(containerId)) {
      acc->second._proxySubscriptions.erase(containerId);
      std::cout << "Grape " << grapeId << " is unsubscribing from container "
                << containerId << " owned by " << ownerOfContainerId
                << std::endl;
      CallGrapeUnsubscribeFromContainer()
          .on_peer(grapeId)
          .on_fail_log_error()
          .with_timeout(std::chrono::milliseconds(1000))
          .retry(3)
          .call<bool>(ownerOfContainerId, containerId);
    }
  }
}

bool GrapeRegistry::SubscribeGrapeToContainer(const std::string &grapeId,
                                              const std::string &containerId,
                                              std::function<void()> onReady) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    acc->second.__subscribeToContainer(containerId, onReady, false);
    return true;
  }
  return false;
}

void GrapeRegistry::UnsubscribeGrapeFromContainer(
    const std::string &grapeId, const std::string &containerId) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    acc->second.__unsubscribeFromContainer(containerId);
  }
}

void GrapeRegistry::ProxyTakeAuthority(const std::string &grapeId,
                                       const std::string &entityId) {
  std::string fromContainerId = ETTREGISTRY.GetEntityOwnerContainer(entityId);
  if (not ContainerRegistry::GetInstance().ContainerIsLocallyOwned(
          fromContainerId)) {
    return;
  }

  std::string payload = ETTREGISTRY.GetEntityPayload(entityId).value_or("{}");
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    // if grape is locally owned, there is no need to take authority through
    // proxy
    if (acc->second.isLocallyOwned) {
      return;
    }
    // todo:  quaranteen ett
    AuthorityTransfer::ProxyTakeAuthority(grapeId, entityId, fromContainerId,
                                          payload);
  }
}

#endif
