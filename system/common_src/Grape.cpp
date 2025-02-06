#include "Container.hpp"
#include "Grape.hpp"
#include "Logger.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"

using namespace celte;

void Grape::initRPCService() {
  {
    std::vector<std::string> topics = {tp::rpc(id)};
    if (isLocallyOwned) {
      topics.push_back(
          tp::peer(id)); // not really a peer but the raw id without
    }
    rpcService.emplace(
        net::RPCService::Options{.thisPeerUuid = RUNTIME.GetUUID(),
                                 .listenOn = topics,
                                 .reponseTopic = tp::peer(RUNTIME.GetUUID()),
                                 .serviceName = tp::rpc(id)});

#ifdef CELTE_SERVER_MODE_ENABLED
    rpcService->Register<std::map<std::string, std::string>>(
        "__rp_getExistingEntities",
        std::function<std::map<std::string, std::string>(std::string)>(
            [this](std::string containerId) {
              return ETTREGISTRY.GetExistingEntities(containerId);
            }));

    rpcService->Register<std::vector<std::string>>(
        "__rp_getExistingOwnedContainers",
        std::function<std::vector<std::string>()>(
            [this]() { return __rp_getExistingOwnedContainers(); }));

    rpcService->Register<bool>("__rp_subscribeToContainer",
                               std::function([this](std::string containerId) {
                                 subscribeToContainer(
                                     containerId, []() {}, false);
                                 return true;
                               }));

    rpcService->Register<bool>("__rp_unsubscribeFromContainer",
                               std::function([this](std::string containerId) {
                                 unsubscribeFromContainer(containerId);
                                 return true;
                               }));
#endif
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::optional<std::string>
Grape::subscribeToContainer(const std::string &containerId,
                            std::function<void()> onReady,
                            bool isLocallyOwned) {
  auto id = containerSubscriptionComponent.Subscribe(containerId, onReady,
                                                     isLocallyOwned);
  if (!id.has_value()) {
    return id;
  }
  if (isLocallyOwned) {
    ownedContainers.insert(id.value());
  } else { // if not locally owned, fetch existing entities from the owner
    ETTREGISTRY.LoadExistingEntities(this->id, containerId);
  }
  return id;
}

void Grape::unsubscribeFromContainer(const std::string &containerId) {
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
        RUNTIME.GetPeerService().GetRPCService().Call<std::vector<std::string>>(
            tp::peer(id), "__rp_getExistingOwnedContainers", id);
    for (auto &containerId : existingContainers) {
      subscribeToContainer(containerId, []() {}, false);
    }
    // } catch (const net::RPCTimeoutException &e) {
    //   fetchExistingContainers();
  } catch (const std::exception &e) {
    std::cerr << "Error fetching existing containers: " << e.what()
              << std::endl;
  }
}

std::vector<std::string> Grape::__rp_getExistingOwnedContainers() {
  std::vector<std::string> result;
  {
    std::lock_guard<std::mutex> lock(ownedContainersMutex);
    for (auto &containerId : ownedContainers) {
      result.push_back(containerId);
    }
  }
  std::cout << "getting existing owned containers in grape " << id
            << ", returning: [\n";
  for (auto &c : result) {
    std::cout << "  - " << c << std::endl;
  }
  std::cout << "]" << std::endl;
  return result;
}
#endif