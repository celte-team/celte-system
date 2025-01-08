#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
namespace chunks {
void Grape::__initNetwork() {
  std::vector<std::string> rpcTopics{tp::PERSIST_DEFAULT + _options.grapeId +
                                     "." + tp::RPCs};
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    rpcTopics.push_back(tp::PERSIST_DEFAULT + _options.grapeId);
  }
#endif

  {
    // debug
    std::cout << "this grape is listening on topics: ";
    for (auto &topic : rpcTopics) {
      std::cout << topic << ", ";
    }
    std::cout << std::endl;
  }

  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = rpcTopics,
      .responseTopic = RUNTIME.GetUUID() + "." + tp::RPCs,
      .serviceName =
          RUNTIME.GetUUID() + ".grape." + _options.grapeId + "." + tp::RPCs,
  });

#ifdef CELTE_SERVER_MODE_ENABLED
  if (_options.isLocallyOwned) {
    _rpcs->Register<std::string>("__rp_sendExistingEntitiesSummary",
                                 std::function([this](std::string chunkId) {
                                   return ENTITIES.GetRegisteredEntitiesSummary(
                                       chunkId);
                                 }));
  }

  _rpcs->Register<bool>(
      "__rp_onSpawnRequested",
      std::function([this](std::string clientId, std::string payload) {
        try {
          __rp_onSpawnRequested(clientId, payload);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_onSpawnRequested: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  _rpcs->Register<bool>(
      "__rp_remoteTakeEntity",
      std::function([this](std::string entityId, std::string callerId,
                           std::string prevOwnerContainerId) {
        return __rp_remoteTakeEntity(entityId, callerId, prevOwnerContainerId);
      }));

  _rpcs->Register<std::string>(
      "__rp_fetchContainerFeatures",
      std::function([this]() { return __rp_fetchContainerFeatures(); }));

#endif

  _rpcs->Register<bool>(
      "__rp_spawnEntity",
      std::function([this](std::string clientId, std::string containerId,
                           std::string payload, float x, float y, float z) {
        __execEntitySpawnProcess(clientId, containerId, payload, x, y, z);
        return true;
      }));

  _rpcs->Register<unsigned int>(
      "GetNumberOfContainers",
      std::function([this]() { return _rg.GetNumberOfContainers(); }));

  _rpcs->Register<bool>("__rp_forceUpdateContainer",
                        std::function([this](std::string containerFeatures) {
                          __rp_forceUpdateContainer(containerFeatures);
                          return true;
                        }));
}

} // namespace chunks
} // namespace celte