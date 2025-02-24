#include "GhostSystem.hpp"
#include "GrapeRegistry.hpp"
#include "Logger.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <iostream>
#ifdef CELTE_SERVER_MODE_ENABLED
#include "MetricsScrapper.hpp"
#endif

using namespace celte;

static std::string make_uuid() {
  boost::uuids::random_generator gen;
  boost::uuids::uuid id = gen();
#ifdef CELTE_SERVER_MODE_ENABLED
  return "sn." + boost::uuids::to_string(id);
#else
  return "cl." + boost::uuids::to_string(id);
#endif
}

Runtime::Runtime() : _uuid(make_uuid()) {}

Runtime &Runtime::GetInstance() {
  static Runtime instance;
  return instance;
}

void Runtime::ConnectToCluster() {
  // getting the address from the environment. if not found, we use localhost
  const char *host = std::getenv("CELTE_HOST");
  const char *port = std::getenv("CELTE_PORT");
  std::string address = host ? host : "localhost";
  if (port) {
    ConnectToCluster(address, std::stoi(port));
  } else {
    ConnectToCluster(address, 6650);
  }
}

void Runtime::ConnectToCluster(const std::string &address, int port) {
  LOGGER.log(Logger::DEBUG, "Connecting to pulsar cluster at " + address + ":" +
                                std::to_string(port));
  net::CelteNet::Instance().Connect(address + ":" + std::to_string(port));
  _peerService = std::make_unique<PeerService>(
      std::function<void(bool)>([this](bool connected) {
        if (!connected) {
          std::cerr << "Error connecting to cluster" << std::endl;
          _hooks.onConnectionFailed();
          return;
        }
        _hooks.onConnectionSuccess();
      }));
#ifdef CELTE_SERVER_MODE_ENABLED
  METRICS.Start(); // metrics should have been registered by now.
  GHOSTSYSTEM.StartReplicationUploadWorker();
#endif
}

void Runtime::Tick() { __advanceSyncTasks(); }

void Runtime::__advanceSyncTasks() {
  std::function<void()> task;
  while (_syncTasks.try_pop(task)) {
    task();
  }
}

void Runtime::RegisterCustomGlobalRPC(
    const std::string &name, std::function<std::string(std::string)> f) {
  if (not _peerService) {
    std::cerr
        << "Peer service not initialized. Please connect to the cluster first."
        << std::endl;
    return;
  }
  _peerService->GetRPCService().Register<std::string>(name, f);
}

void Runtime::CallScopedRPCNoRetVal(const std::string &scope,
                                    const std::string &name,
                                    const std::string &args) {
  if (not _peerService) {
    std::cerr << "Peer service not initialized. Please connect to the cluster "
                 "first."
              << std::endl;
    return;
  }
  _peerService->GetRPCService().CallVoid(tp::default_scope + scope, name, args);
}

std::string Runtime::CallScopedRPC(const std::string &scope,
                                   const std::string &name,
                                   const std::string &args) {
  if (not _peerService) {
    std::cerr << "Peer service not initialized. Please connect to the cluster "
                 "first."
              << std::endl;
    return "";
  }
  return _peerService->GetRPCService().Call<std::string>(
      tp::default_scope + scope, name, args);
}

void Runtime::CallScopedRPCAsync(const std::string &scope,
                                 const std::string &name,
                                 const std::string &args,
                                 std::function<void(std::string)> callback) {
  if (not _peerService) {
    std::cerr << "Peer service not initialized. Please connect to the cluster "
                 "first."
              << std::endl;
    return;
  }
  _peerService->GetRPCService()
      .CallAsync<std::string>(tp::default_scope + scope, name, args)
      .Then(callback);
}

#ifdef CELTE_SERVER_MODE_ENABLED

void Runtime::MasterInstantiateServerNode(const std::string &payload) {
  throw std::runtime_error("Not implemented");
}

void Runtime::ForceDisconnectClient(const std::string &clientId,
                                    const std::string &payload) {
  // we send the final disconnect message to this grape rpc channel
  _peerService->GetRPCService().CallVoid(tp::rpc(RUNTIME.GetAssignedGrape()),
                                         "__rp_execClientDisconnect", clientId,
                                         payload);
}

#else

void Runtime::Disconnect() {
  // we send the disconnect message to all grapes
  for (auto &g : GRAPES.GetGrapes()) {
    g.second.rpcService->CallVoid(
        tp::peer(g.second.id), // tp::peer because msg is for the server, not
                               // everyone in the grape
        "__rp_requestClientDisconnect", _uuid);
  }
}
#endif
