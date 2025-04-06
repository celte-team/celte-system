#include "AuthorityTransfer.hpp"
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "CRPC.hpp"
#include "Container.hpp"
#include "Grape.hpp"
#include "GrapeRegistry.hpp"
#include "Logger.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"

using namespace celte;

void Grape::initRPCService() {
  {
#ifdef CELTE_SERVER_MODE_ENABLED

    if (isLocallyOwned) {
      GrapeGetExistingEntitiesReactor::subscribe(tp::peer(id), this);
      GrapeGetExistingOwnedContainersReactor::subscribe(tp::peer(id), this);
      GrapeSubscribeToContainerReactor::subscribe(tp::peer(id), this);
      GrapeUnsubscribeFromContainerReactor::subscribe(tp::peer(id), this);
      GrapeProxyTakeAuthorityReactor::subscribe(tp::peer(id), this);
      GrapeRequestClientDisconnectReactor::subscribe(tp::peer(id), this);
      GrapePingReactor::subscribe(tp::peer(id), this);
      GrapeCMIInstantiateReactor::subscribe(tp::peer(id), this);
    }

#endif

    GrapeExecClientDisconnectReactor::subscribe(tp::rpc(id), this);
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::map<std::string, std::string>
Grape::GetExistingEntities(std::string containerId) {
  auto result = ETTREGISTRY.GetExistingEntities(containerId);
  return result;
}

bool Grape::SubscribeToContainer(std::string ownerOfContainerId,
                                 std::string containerId) {
  GRAPES.RunWithLock(ownerOfContainerId, [containerId](Grape &g) {
    g.__subscribeToContainer(containerId, []() {}, false);
  });
  return true;
}

bool Grape::UnsubscribeFromContainer(std::string ownerOfContainerId,
                                     std::string containerId) {
  GRAPES.RunWithLock(ownerOfContainerId, [containerId](Grape &g) {
    g.__unsubscribeFromContainer(containerId);
  });
  return true;
}

bool Grape::ProxyTakeAuthority(std::string entityId,
                               std::string fromContainerId,
                               std::string payload) {
  AuthorityTransfer::__rp_proxyTakeAuthority(id, entityId, fromContainerId,
                                             payload);
  return true;
}

bool Grape::RequestClientDisconnect(std::string clientId) {
  RUNTIME.Hooks().onClientRequestDisconnect(clientId);
  return true;
}

bool Grape::Ping() { return true; }

// this has no lock. if you need a lock, use the public binding
// SubscribeToContainer
std::optional<std::string>
Grape::__subscribeToContainer(const std::string &containerId,
                              std::function<void()> onReady,
                              bool isLocallyOwned) {
  auto id = containerSubscriptionComponent.Subscribe(containerId, onReady,
                                                     isLocallyOwned);
  if (!id.has_value()) {
    return id;
  }
  if (isLocallyOwned) {
    ownedContainers[id.value()] = std::nullopt;
  } else { // if not locally owned, fetch existing entities from the owner
    ETTREGISTRY.LoadExistingEntities(this->id, containerId);
  }
  return id;
}

void Grape::__unsubscribeFromContainer(const std::string &containerId) {
  // if container is locally owned, do not unsubscribe. If not needed, the
  // container should be manually deleted but this is not the place to do it.
  if (ContainerRegistry::GetInstance().ContainerIsLocallyOwned(containerId)) {
    return;
  }
  containerSubscriptionComponent.Unsubscribe(containerId);
}

void Grape::fetchExistingContainers() {
#ifdef CELTE_SERVER_MODE_ENABLED
  if (id == RUNTIME.GetAssignedGrape()) {
    return;
  }
#endif
  try {
    LOGINFO("Fetching existing containers in grape " + id);
    std::vector<std::string> existingContainers =
        CallGrapeGetExistingOwnedContainers()
            .on_peer(id)
            .on_fail_log_error()
            .with_timeout(std::chrono::milliseconds(1000))
            .retry(3)
            .call<std::vector<std::string>>(id)
            .value_or(std::vector<std::string>{}); // empty vector if failed
    for (auto &containerId : existingContainers) {
      __subscribeToContainer(containerId, []() {}, false);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error fetching existing containers: " << e.what()
              << std::endl;
  }
}

std::vector<std::string> Grape::GetExistingOwnedContainers() {
  std::vector<std::string> result;
  {
    std::lock_guard<std::mutex> lock(ownedContainersMutex);
    for (auto &[containerId, _] : ownedContainers) {
      result.push_back(containerId);
    }
  }
  return result;
}
#endif

bool Grape::ExecClientDisconnect(std::string clientId, std::string payload) {
  RUNTIME.Hooks().onClientDisconnect(clientId, payload);
  __cleanupClientData(clientId);
  return true;
}

void Grape::__cleanupClientData(const std::string &clientId) {
#ifdef CELTE_SERVER_MODE_ENABLED
  RUNTIME.GetPeerService().GetClientRegistry().ForgetClient(clientId);
#else
#endif
}

#ifdef CELTE_SERVER_MODE_ENABLED
void Grape::CMIInstantiate(std::string cmiId, std::string prefabId,
                           std::string payload, std::string clientId) {
  GRAPES.PushNamedTaskToEngine(
      id,"CMIInstantiate", cmiId, prefabId, payload, clientId);
}
#endif