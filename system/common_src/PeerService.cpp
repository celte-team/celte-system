#include "AuthorityTransfer.hpp"
#include "Clock.hpp"
#include "GrapeRegistry.hpp"
#include "HttpClient.hpp"
#include "PeerService.hpp"

#include "Runtime.hpp"
#include "Topics.hpp"
#include "systems_structs.pb.h"
#include <functional>
#include <future>

using namespace celte;
PeerService::PeerService(std::function<void(bool)> onReady,
                         std::chrono::milliseconds connectionTimeout)
    : _wspool({.idleTimeout = 10000ms}) {

  std::cout << "Listening on " << tp::rpc(RUNTIME.GetUUID()) << " and "
            << tp::rpc(tp::global_rpc()) << std::endl;

  CLOCK.Start();
  RUNTIME.ScheduleAsyncTask([this, onReady, connectionTimeout]() {
    if (!__waitNetworkReady(connectionTimeout)) {
      onReady(false);
      return;
    }
    __initPeerRPCs();
    onReady(true);
  });
}

PeerService::~PeerService() { CLOCK.Stop(); }

bool PeerService::__waitNetworkReady(
    std::chrono::milliseconds connectionTimeout) {
  // Network init is synchronous now, will change in the future so we keep this
  // method
  return true;
}

void PeerService::__initPeerRPCs() {
#ifdef CELTE_SERVER_MODE_ENABLED
  __registerServerRPCs();
#else
  __registerClientRPCs();
#endif
}

#ifdef CELTE_SERVER_MODE_ENABLED
void PeerService::__registerServerRPCs() {
  auto id = RUNTIME.GetUUID();
  PeerServiceAssignGrapeReactor::subscribe(tp::rpc(id), this);
  PeerServiceRequestSpawnPositionReactor::subscribe(tp::rpc(id), this);
  PeerServiceAcceptNewClientReactor::subscribe(tp::rpc(id), this);
}

#else
void PeerService::__registerClientRPCs() {
  PeerServiceForceConnectToNodeReactor::subscribe(tp::peer(RUNTIME.GetUUID()),
                                                  this);
  PeerServiceSubscribeClientToContainerReactor::subscribe(
      tp::peer(RUNTIME.GetUUID()), this);
  PeerServiceUnsubscribeClientFromContainerReactor::subscribe(
      tp::peer(RUNTIME.GetUUID()), this);
  PeerServicePingReactor::subscribe(tp::peer(RUNTIME.GetUUID()), this);
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

bool PeerService::AcceptNewClient(std::string clientId, std::string spawnerId) {
  RUNTIME.GetPeerService().GetClientRegistry().RegisterClient(clientId, "", "");
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(),
                     [this, clientId, spawnerId](Grape &g) {
                       g.executor.PushTaskToEngine([clientId, spawnerId]() {
                         RUNTIME.Hooks().onAcceptNewClient(clientId, spawnerId);
                       });
                     });
  return true;
}

void PeerService::ConnectClientToThisNode(const std::string &clientId,
                                          std::function<void()> then) {
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
            then();
            return;
          }
          std::cout << "client " << clientId.substr(0, 7)
                    << "\033[032m <- \033[0m" << containerId.substr(0, 4)
                    << std::endl;
          c.remoteClientSubscriptions.insert(containerId);
          RUNTIME.ScheduleAsyncIOTask([this, then, clientId, containerId]() {
            LOGINFO("Subscribing client " + clientId + " to container " +
                    containerId);
            CallPeerServiceSubscribeClientToContainer()
                .on_peer(clientId)
                .on_fail_log_error()
                .with_timeout(std::chrono::milliseconds(1000))
                .retry(3)
                .call_async<bool>(
                    [then, containerId](bool ok) {
                      std::cout << "Client was subscribed to container "
                                << containerId.substr(0, 4) << " by grape "
                                << RUNTIME.GetAssignedGrape().substr(0, 7)
                                << std::endl;
                      if (ok) {
                        then();
                      } else {
                        LOGERROR("Error subscribing client to container");
                      }
                    },
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
#ifdef DEBUG
          std::cout << "client " << clientId.substr(0, 7)
                    << "\033[031m x- \033[0m" << containerId.substr(0, 4)
                    << std::endl;
#endif
          c.remoteClientSubscriptions.erase(containerId);

          RUNTIME.ScheduleAsyncIOTask([this, clientId, containerId]() {
            LOGINFO("Unsubscribing client " + clientId + " from container " +
                    containerId);
            bool sucess = CallPeerServiceUnsubscribeClientFromContainer()
                              .on_peer(clientId)
                              .on_fail_log_error()
                              .with_timeout(std::chrono::milliseconds(1000))
                              .retry(3)
                              // .fire_and_forget(containerId);
                              .call<bool>(containerId)
                              .value_or(false);
            if (!sucess) {
              LOGERROR("Error unsubscribing client from container");
              std::cerr << "Error unsubscribing client from container"
                        << std::endl;
            }
          });
        });
  });
}

#else

bool PeerService::ForceConnectToNode(std::string grapeId) {
  std::cout << "FORCE CONNECT TO NODE WAS CALLED FOR GRAPE "
            << grapeId.substr(0, 7) << std::endl;
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
#ifdef DEBUG
  std::cout << "self(" + RUNTIME.GetUUID().substr(0, 7) + ")\033[31m -x \033[0m"
            << containerId.substr(0, 4) << std::endl;
#endif
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
