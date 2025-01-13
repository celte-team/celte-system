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
    : _rpcService(
          net::RPCService::Options{.thisPeerUuid = RUNTIME.GetUUID(),
                                   .listenOn = {tp::rpc(RUNTIME.GetUUID())},
                                   .reponseTopic = tp::peer(RUNTIME.GetUUID()),
                                   .serviceName = tp::peer(RUNTIME.GetUUID())}),
      _wspool({.idleTimeout = 10000ms}) {
  RUNTIME.ScheduleAsyncTask([this, onReady, connectionTimeout]() {
    if (!__waitNetworkReady(connectionTimeout)) {
      onReady(false);
      return;
    }
    __initPeerRPCs();
    __pingMaster(onReady);
  });
}

PeerService::~PeerService() { _rpcService.reset(); }

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
}

#else
void PeerService::__registerClientRPCs() {
  _rpcService->Register<bool>("__rp_forceConnectToChunk",
                              std::function([this](std::string grapeId) {
                                return __rp_forceConnectToGrape(grapeId);
                              }));
}
#endif

#ifdef CELTE_SERVER_MODE_ENABLED
bool PeerService::__rp_assignGrape(const std::string &grapeId) {
  std::cout << "Assigning grape " << grapeId << std::endl;
  return true;
}

#else

bool PeerService::__rp_forceConnectToGrape(const std::string &grapeId) {
  std::cout << "Force connecting to grape " << grapeId << std::endl;
  return true;
}

#endif
