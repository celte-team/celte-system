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
    : //  _rpcService(net::RPCService::Options{
      //       .thisPeerUuid = RUNTIME.GetUUID(),
      //       .listenOn = {tp::rpc(RUNTIME.GetUUID()),
      //       tp::rpc(tp::global_rpc)}, .reponseTopic =
      //       tp::rpc(RUNTIME.GetUUID()), .serviceName =
      //       tp::peer(RUNTIME.GetUUID())}),
      _wspool({.idleTimeout = 10000ms}) {

  std::cout << "Listening on " << tp::rpc(RUNTIME.GetUUID()) << " and "
            << tp::rpc(tp::global_rpc) << std::endl;

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
  // _rpcService.reset();
  CLOCK.Stop();
}

bool PeerService::__waitNetworkReady(
    std::chrono::milliseconds connectionTimeout) {
  auto start = std::chrono::steady_clock::now();
  // while (!_rpcService->Ready()) {
  //   if (std::chrono::steady_clock::now() - start > connectionTimeout) {
  //     std::cerr << "Timeout waiting for network to be ready" << std::endl;
  //     return false;
  //   }
  // }
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
  // _rpcService->Register<bool>("__rp_assignGrape",
  //                             std::function([this](std::string grapeId) {
  //                               return __rp_assignGrape(grapeId);
  //                             }));

  PeerServiceAssignGrapeReactor::subscribe(tp::rpc(RUNTIME.GetUUID()), this);

  // _rpcService->Register<std::string>(
  //     "__rp_getPlayerSpawnPosition",
  //     std::function([this](std::string clientId) {
  //       return __rp_spawnPositionRequest(clientId);
  //     }));

  PeerServiceRequestSpawnPositionReactor::subscribe(tp::rpc(RUNTIME.GetUUID()),
                                                    this);

  // _rpcService->Register<bool>("__rp_acceptNewClient",
  //                             std::function([this](std::string clientId) {
  //                               return __rp_acceptNewClient(clientId);
  //                             }));

  PeerServiceAcceptNewClientReactor::subscribe(tp::rpc(RUNTIME.GetUUID()),
                                               this);
}

#else
void PeerService::__registerClientRPCs() {
  // _rpcService->Register<bool>("__rp_forceConnectToNode",
  //                             std::function([this](std::string nodeId) {
  //                               return __rp_forceConnectToNode(nodeId);
  //                             }));

  PeerServiceForceConnectToNodeReactor::subscribe(tp::peer(RUNTIME.GetUUID()),
                                                  this);

  // _rpcService->Register<bool>(
  //     "__rp_subscribeClientToContainer",
  //     std::function([this](std::string containerId, std::string ownerGrapeId)
  //     {
  //       return __rp_subscribeClientToContainer(containerId, ownerGrapeId);
  //     }));

  PeerServiceSubscribeClientToContainerReactor::subscribe(
      tp::peer(RUNTIME.GetUUID()), this);

  // _rpcService->Register<bool>(
  //     "__rp_unsubscribeClient", std::function([this](std::string containerId)
  //     {
  //       return __rp_unsubscribeClientFromContainer(containerId);
  //     }));

  PeerServiceUnsubscribeClientFromContainerReactor::subscribe(
      tp::peer(RUNTIME.GetUUID()), this);

  // _rpcService->Register<bool>(
  //     "__rp_ping", std::function([this](bool) { return __rp_ping(); }));

  PeerServicePingReactor::subscribe(tp::rpc(RUNTIME.GetUUID()), this);
}
#endif

#ifdef CELTE_SERVER_MODE_ENABLED
bool PeerService::AssignGrape(std::string grapeId) {
  LOGINFO("Taking ownership of grape " + grapeId);
  RUNTIME.SetAssignedGrape(grapeId);
  RUNTIME.TopExecutor().PushTaskToEngine(
      [grapeId]() { RUNTIME.Hooks().onLoadGrape(grapeId, true); });
  return true;
}

std::string PeerService::RequestSpawnPosition(std::string clientId) {
  std::string grapeId = RUNTIME.Hooks().onGetClientInitialGrape(clientId);
  nlohmann::json j = {{"grapeId", grapeId}, {"clientId", clientId}};
  return j.dump();
}

bool PeerService::AcceptNewClient(std::string clientId) {
  RUNTIME.GetPeerService().GetClientRegistry().RegisterClient(clientId, "", "");
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(), [this, clientId](Grape &g) {
    g.executor.PushTaskToEngine(
        [clientId]() { RUNTIME.Hooks().onAcceptNewClient(clientId); });
  });
  return true;
}

void PeerService::ConnectClientToThisNode(const std::string &clientId,
                                          std::function<void()> then) {
  // bool ok =
  // _rpcService->Call<bool>(tp::rpc(clientId),
  // "__rp_forceConnectToNode",
  //                         RUNTIME.GetAssignedGrape());
  bool ok = CallPeerServiceForceConnectToNode()
                .on_peer(clientId)
                .on_fail_log_error()
                .with_timeout(std::chrono::milliseconds(1000))
                .retry(3)
                .call<bool>(RUNTIME.GetAssignedGrape())
                .value_or(false);
  if (ok) {
    RUNTIME.TopExecutor().PushTaskToEngine(then);
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
            // _rpcService->CallVoid(tp::rpc(clientId),
            //                       "__rp_subscribeClientToContainer",
            //                       containerId, RUNTIME.GetAssignedGrape());
            CallPeerServiceSubscribeClientToContainer()
                .on_peer(clientId)
                .on_fail_log_error()
                .with_timeout(std::chrono::milliseconds(1000))
                .retry(3)
                .fire_and_forget(containerId, RUNTIME.GetAssignedGrape());
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
            // _rpcService->CallVoid(tp::rpc(clientId),
            // "__rp_unsubscribeClient",
            //                       containerId);
            CallPeerServiceUnsubscribeClientFromContainer()
                .on_peer(clientId)
                .on_fail_log_error()
                .with_timeout(std::chrono::milliseconds(1000))
                .retry(3)
                .fire_and_forget(containerId);
          });
        });
  });
}

#else

bool PeerService::ForceConnectToNode(std::string grapeId) {
  RUNTIME.ScheduleAsyncTask(
      [grapeId]() { RUNTIME.Hooks().onLoadGrape(grapeId, false); });
  return true;
}

bool PeerService::SubscribeClientToContainer(std::string containerId,
                                             std::string ownerGrapeId) {
  std::cout << "self(" + RUNTIME.GetUUID().substr(0, 7) + ")\033[32m -> \033[0m"
            << containerId.substr(0, 4) << std::endl;
  _containerSubscriptionComponent.Subscribe(containerId, []() {}, false);
  ETTREGISTRY.LoadExistingEntities(ownerGrapeId, containerId);
  return true;
}

bool PeerService::UnsubscribeClientFromContainer(std::string containerId) {
  std::cout << "self(" + RUNTIME.GetUUID().substr(0, 7) + ")\033[31m -x \033[0m"
            << containerId.substr(0, 4) << std::endl;
  _containerSubscriptionComponent.Unsubscribe(containerId);
  return true;
}

#endif

bool PeerService::Ping() { return true; }

std::map<std::string, int> PeerService::GetLatency() {
  std::map<std::string, int> latencies;
  std::vector<std::string> grapes = GRAPES.GetKnownGrapes();
  for (const auto &g : grapes) {
    auto start = std::chrono::steady_clock::now();
    // _rpcService->Call<bool>(tp::peer(g), "__rp_ping", true);
    CallGrapePing()
        .on_peer(g)
        .on_fail_do([&g, &latencies](auto &e) { latencies[g] = -1; })
        .with_timeout(std::chrono::milliseconds(1000))
        .retry(3)
        .call<bool>();
    auto end = std::chrono::steady_clock::now();
    latencies[g] =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
  }
  return latencies;
}