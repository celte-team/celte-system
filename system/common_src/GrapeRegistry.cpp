#include "AuthorityTransfer.hpp"
#include "GrapeRegistry.hpp"
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

  acc->second.id = grapeId;
  acc->second.isLocallyOwned = isLocallyOwned;
#ifdef CELTE_SERVER_MODE_ENABLED
  if (isLocallyOwned) {
    acc->second.clientRegistry.emplace(); // create the client registry
    acc->second.clientRegistry->StartKeepAliveThread();
  }
#endif
  acc.release();

  if (onReady) {
    RUNTIME.ScheduleAsyncTask([onReady, grapeId, isLocallyOwned]() {
      // wait for the grape's network service to be ready
      accessor acc2;
      if (GRAPES.GetGrapes().find(acc2, grapeId)) {
        acc2->second.initRPCService();
        while (!acc2->second.rpcService.has_value() and
               not acc2->second.rpcService->Ready()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#ifdef CELTE_SERVER_MODE_ENABLED
        if (not isLocallyOwned) {
          std::cout << "grape " << grapeId
                    << " is fetching existing containers because it is not "
                       "locally owned. "
                    << std::endl;
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
    std::cout << std::endl
              << "-------------------------------------" << std::endl
              << "grape " << grapeId << " is creating a new container with id "
              << id << " and isLocallyOwned: " << acc->second.isLocallyOwned
              << std::endl
              << std::endl;
    auto finalId = acc->second
                       .subscribeToContainer(
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
void GrapeRegistry::SetRemoteGrapeSubscription(const std::string &grapeId,
                                               const std::string &containerId,
                                               bool subscribe) {
  if (grapeId == RUNTIME.GetAssignedGrape()) {
    return;
  }
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    if (subscribe && not acc->second._proxySubscriptions.count(containerId)) {
      acc->second._proxySubscriptions.insert(containerId);
      acc->second.rpcService->CallVoid(
          tp::peer(grapeId), "__rp_subscribeToContainer", containerId);
    } else if (not subscribe and
               acc->second._proxySubscriptions.count(containerId)) {
      acc->second._proxySubscriptions.erase(containerId);
      acc->second.rpcService->CallVoid(
          tp::peer(grapeId), "__rp_unsubscribeFromContainer", containerId);
    }
  }
}

bool GrapeRegistry::SubscribeGrapeToContainer(const std::string &grapeId,
                                              const std::string &containerId,
                                              std::function<void()> onReady) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    acc->second.subscribeToContainer(containerId, onReady, false);
    std::cout << "Subscribed grape " << grapeId << " to container "
              << containerId << std::endl;
    return true;
  }
  std::cout << "Grape not found: " << grapeId << std::endl;
  return false;
}

void GrapeRegistry::UnsubscribeGrapeFromContainer(
    const std::string &grapeId, const std::string &containerId) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    acc->second.unsubscribeFromContainer(containerId);
  }
}
#endif

void GrapeRegistry::ProxyTakeAuthority(const std::string &grapeId,
                                       const std::string &entityId,
                                       const std::string &fromContainerId,
                                       const std::string &payload) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    // todo:  quaranteen ett
    AuthorityTransfer::ProxyTakeAuthority(grapeId, entityId, fromContainerId,
                                          payload);
  } else {
    std::cout << "Grape not found: " << grapeId
              << ", cannot take authority through proxy" << std::endl;
  }
}