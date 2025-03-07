#include "AuthorityTransfer.hpp"
#include "Clock.hpp"
#include "GrapeRegistry.hpp"
#include "PeerService.hpp"
#include "RPCService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include "systems_structs.pb.h"
#include <functional>
#include <future>

using namespace celte;

PeerService::PeerService(std::function<void(bool)> onReady,
                         std::chrono::milliseconds connectionTimeout)
    : _rpcService(net::RPCService::Options{
          .thisPeerUuid = RUNTIME.GetUUID(),
          .listenOn = {tp::rpc(RUNTIME.GetUUID()), tp::global_rpc},
          .reponseTopic = tp::rpc(RUNTIME.GetUUID()),
          .serviceName = tp::peer(RUNTIME.GetUUID())}),
      _wspool({.idleTimeout = 10000ms}) {

  std::cout << "Listening on " << tp::rpc(RUNTIME.GetUUID()) << " and "
            << tp::global_rpc << std::endl;

  CLOCK.Start();
  RUNTIME.ScheduleAsyncTask([this, onReady, connectionTimeout]() {
    if (!__waitNetworkReady(connectionTimeout)) {
      onReady(false);
      return;
    }
    __initPeerRPCs();
    __pingMaster(onReady);
  });
}

PeerService::~PeerService() {
  _rpcService.reset();
  CLOCK.Stop();
}

bool PeerService::__waitNetworkReady(
    std::chrono::milliseconds connectionTimeout) {
  auto start = std::chrono::steady_clock::now();
  while (!_rpcService->Ready()) {
    if (std::chrono::steady_clock::now() - start > connectionTimeout) {
      std::cerr << "Timeout waiting for network to be ready" << std::endl;
      return false;
    }
  }
  return true;
}

void PeerService::__initPeerRPCs() {
#ifdef CELTE_SERVER_MODE_ENABLED
  __registerServerRPCs();
#else
  __registerClientRPCs();
#endif
}

void PeerService::__pingMaster(std::function<void(bool)> onReady) {
  req::BinaryDataPacket req;
  req.set_binarydata(RUNTIME.GetUUID());
  req.set_peeruuid(RUNTIME.GetUUID());

#ifdef CELTE_SERVER_MODE_ENABLED
  const std::string topic = tp::hello_master_sn;
#else
  const std::string topic = tp::hello_master_cl;
#endif

  _wspool.Write(topic, req, [onReady](pulsar::Result r) {
    if (r != pulsar::ResultOk) {
      std::cerr << "Error connecting to master server" << std::endl;
      onReady(false);
    } else {
      onReady(true);
    }
  });
}

#ifdef CELTE_SERVER_MODE_ENABLED
void PeerService::__registerServerRPCs() {
  _rpcService->Register<bool>("__rp_assignGrape",
                              std::function([this](std::string grapeId) {
                                return __rp_assignGrape(grapeId);
                              }));

  _rpcService->Register<std::string>(
      "__rp_getPlayerSpawnPosition",
      std::function([this](std::string clientId) {
        return __rp_spawnPositionRequest(clientId);
      }));

  _rpcService->Register<bool>("__rp_acceptNewClient",
                              std::function([this](std::string clientId) {
                                return __rp_acceptNewClient(clientId);
                              }));
}

#else

void PeerService::__registerClientRPCs() {
  _rpcService->Register<bool>("__rp_forceConnectToNode",
                              std::function([this](std::string nodeId) {
                                return __rp_forceConnectToNode(nodeId);
                              }));

  _rpcService->Register<bool>(
      "__rp_subscribeClientToContainer",
      std::function([this](std::string containerId, std::string ownerGrapeId) {
        return __rp_subscribeClientToContainer(containerId, ownerGrapeId);
      }));

  _rpcService->Register<bool>(
      "__rp_unsubscribeClient", std::function([this](std::string containerId) {
        return __rp_unsubscribeClientFromContainer(containerId);
      }));

  _rpcService->Register<bool>(
      "__rp_ping", std::function([this](bool) { return __rp_ping(); }));
}
#endif

#ifdef CELTE_SERVER_MODE_ENABLED
bool PeerService::__rp_assignGrape(const std::string &grapeId) {
  LOGINFO("Taking ownership of grape " + grapeId);
  RUNTIME.SetAssignedGrape(grapeId);
  RUNTIME.TopExecutor().PushTaskToEngine(
      [grapeId]() { RUNTIME.Hooks().onLoadGrape(grapeId, true); });
  return true;
}

std::string
PeerService::__rp_spawnPositionRequest(const std::string &clientId) {
  std::string grapeId = RUNTIME.Hooks().onGetClientInitialGrape(clientId);
  nlohmann::json j = {{"grapeId", grapeId}, {"clientId", clientId}};
  return j.dump();
}

bool PeerService::__rp_acceptNewClient(const std::string &clientId) {
  RUNTIME.GetPeerService().GetClientRegistry().RegisterClient(clientId, "", "");
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(), [this, clientId](Grape &g) {
    g.executor.PushTaskToEngine(
        [clientId]() { RUNTIME.Hooks().onAcceptNewClient(clientId); });
  });
  return true;
}

void PeerService::ConnectClientToThisNode(const std::string &clientId,
                                          std::function<void()> then) {
  try {
    bool ok =
        _rpcService->Call<bool>(tp::rpc(clientId), "__rp_forceConnectToNode",
                                RUNTIME.GetAssignedGrape());
    if (ok) {
      RUNTIME.TopExecutor().PushTaskToEngine(then);
    }
  } catch (net::RPCTimeoutException &e) {
    std::cerr << "Error connecting client to this node: " << e.what()
              << std::endl;
  }
}

// this is ran in server mode, to subscribe a client to a container locally
// owned by this grape.
void PeerService::SubscribeClientToContainer(const std::string &clientId,
                                             const std::string &containerId,
                                             std::function<void()> then) {
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(), [this, clientId, containerId,
                                                  then](Grape &g) {
    if (not ContainerRegistry::GetInstance().ContainerIsLocallyOwned(
            containerId)) {
      LOGERROR("Error: container " + containerId +
               " is not locally owned, could not subscribe client to it.");
      return;
    }
    RUNTIME.GetPeerService().GetClientRegistry().RunWithLock(
        clientId, [&](ClientData &c) {
          if (c.isSubscribedToContainer(containerId)) {
            return;
          }
          std::cout << "client " << clientId.substr(0, 7)
                    << "\033[032m <- \033[0m" << containerId.substr(0, 4)
                    << std::endl;
          c.remoteClientSubscriptions.insert(containerId);

          RUNTIME.ScheduleAsyncIOTask([this, clientId, containerId]() {
            LOGINFO("Subscribing client " + clientId + " to container " +
                    containerId);
            _rpcService->CallVoid(tp::rpc(clientId),
                                  "__rp_subscribeClientToContainer",
                                  containerId, RUNTIME.GetAssignedGrape());
          });
        });
  });
}

void PeerService::UnsubscribeClientFromContainer(
    const std::string &clientId, const std::string &containerId) {
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(), [this, clientId,
                                                  containerId](Grape &g) {
    if (not ContainerRegistry::GetInstance().ContainerIsLocallyOwned(
            containerId)) {
      LOGERROR("Error: container " + containerId +
               " is not locally owned, could not unsubscribe client from it.");
      return;
    }
    RUNTIME.GetPeerService().GetClientRegistry().RunWithLock(
        clientId, [&](ClientData &c) {
          if (not c.isSubscribedToContainer(containerId)) {
            return;
          }
          std::cout << "client " << clientId.substr(0, 7)
                    << "\033[031m x- \033[0m" << containerId.substr(0, 4)
                    << std::endl;
          c.remoteClientSubscriptions.erase(containerId);

          RUNTIME.ScheduleAsyncIOTask([this, clientId, containerId]() {
            LOGINFO("Unsubscribing client " + clientId + " from container " +
                    containerId);
            _rpcService->CallVoid(tp::rpc(clientId), "__rp_unsubscribeClient",
                                  containerId);
          });
        });
  });
}

#else

bool PeerService::__rp_forceConnectToNode(const std::string &grapeId) {
  RUNTIME.ScheduleAsyncTask(
      [grapeId]() { RUNTIME.Hooks().onLoadGrape(grapeId, false); });
  return true;
}

bool PeerService::__rp_subscribeClientToContainer(
    const std::string &containerId, const std::string &ownerGrapeId) {
  std::cout << "self(" + RUNTIME.GetUUID().substr(0, 7) + ")\033[32m -> \033[0m"
            << containerId.substr(0, 4) << std::endl;
  _containerSubscriptionComponent.Subscribe(containerId, []() {}, false);
  ETTREGISTRY.LoadExistingEntities(ownerGrapeId, containerId);
  return true;
}

bool PeerService::__rp_unsubscribeClientFromContainer(
    const std::string &containerId) {
  std::cout << "self(" + RUNTIME.GetUUID().substr(0, 7) + ")\033[31m -x \033[0m"
            << containerId.substr(0, 4) << std::endl;
  _containerSubscriptionComponent.Unsubscribe(containerId);
  return true;
}

#endif

bool PeerService::__rp_ping() { return true; }

std::map<std::string, int> PeerService::GetLatency() {
  std::map<std::string, int> latencies;
  std::vector<std::string> grapes = GRAPES.GetKnownGrapes();
  for (const auto &g : grapes) {
    try {
      auto start = std::chrono::steady_clock::now();
      _rpcService->Call<bool>(tp::peer(g), "__rp_ping", true);
      auto end = std::chrono::steady_clock::now();
      latencies[g] =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();
    } catch (net::RPCTimeoutException &e) {
      latencies[g] = -1;
    }
  }
  return latencies;
}