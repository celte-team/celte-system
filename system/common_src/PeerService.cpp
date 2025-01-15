#include "Clock.hpp"
#include "GrapeRegistry.hpp"
#include "PeerService.hpp"
#include "RPCService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include "protos/systems_structs.pb.h"
#include <functional>
#include <future>

using namespace celte;

PeerService::PeerService(std::function<void(bool)> onReady,
                         std::chrono::milliseconds connectionTimeout)
    : _rpcService(net::RPCService::Options{
          .thisPeerUuid = RUNTIME.GetUUID(),
          .listenOn = {tp::rpc(RUNTIME.GetUUID()), tp::global_rpc},
          .reponseTopic = tp::peer(RUNTIME.GetUUID()),
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
  std::cout << "Peer network is ready" << std::endl;
  return true;
}

void PeerService::__initPeerRPCs() {
#ifdef CELTE_SERVER_MODE_ENABLED
  std::cout << "[[SERVER MODE]]" << std::endl;
  __registerServerRPCs();
#else
  std::cout << "[[CLIENT MODE]]" << std::endl;
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
        std::cout << "Requesting spawn position for client " << clientId
                  << std::endl;
        return __rp_spawnPositionRequest(clientId);
      }));

  _rpcService->Register<bool>("__rp_acceptNewClient",
                              std::function([this](std::string clientId) {
                                return __rp_acceptNewClient(clientId);
                              }));
}

#else
void PeerService::__registerClientRPCs() {}
#endif

#ifdef CELTE_SERVER_MODE_ENABLED
bool PeerService::__rp_assignGrape(const std::string &grapeId) {
  std::cout << "Assigning grape " << grapeId << std::endl;
  RUNTIME.SetAssignedGrape(grapeId);
  RUNTIME.TopExecutor().PushTaskToEngine(
      [grapeId]() { RUNTIME.Hooks().onLoadGrape(grapeId, true); });
  return true;
}

std::string
PeerService::__rp_spawnPositionRequest(const std::string &clientId) {
  std::string grapeId = RUNTIME.Hooks().onGetClientInitialGrape(clientId);
  nlohmann::json j = {{"grapeId", grapeId}, {"clientId", clientId}};
  std::cout << "returning " << j.dump() << std::endl;
  return j.dump();
}

bool PeerService::__rp_acceptNewClient(const std::string &clientId) {
  GRAPES.RunWithLock(RUNTIME.GetAssignedGrape(), [clientId](Grape &g) {
    g.clientRegistry->RegisterClient(clientId, g.id, true);
    g.executor.PushTaskToEngine(
        [clientId]() { RUNTIME.Hooks().onAcceptNewClient(clientId); });
  });

  return true;
}

#else

#endif