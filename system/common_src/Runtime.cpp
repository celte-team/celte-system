#include "CRPC.hpp"
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
  int iport = port ? std::stoi(port) : 6650;

  std::cout << "Connecting to pulsar cluster at " << address << ":" << iport
            << std::endl;
  ConnectToCluster(address, iport);
}

void Runtime::ConnectToCluster(const std::string &address, int port) {
  LOGGER.log(Logger::DEBUG, "Connecting to pulsar cluster at " + address + ":" +
                                std::to_string(port));
  net::CelteNet::Instance().Connect(address + ":" + std::to_string(port));
  RPCCalleeStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().StartListeningForAnswers();

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
  std::cout << "RegisterCustomGlobalRPC not implemented yet" << std::endl;
}

void Runtime::CallScopedRPCNoRetVal(const std::string &scope,
                                    const std::string &name,
                                    const std::string &args) {
  std::cout << "CallScopedRPCNoRetVal not implemented yet" << std::endl;
}

std::string Runtime::CallScopedRPC(const std::string &scope,
                                   const std::string &name,
                                   const std::string &args) {
  std::cout << "CallScopedRPC not implemented yet" << std::endl;
  return "";
}

void Runtime::CallScopedRPCAsync(const std::string &scope,
                                 const std::string &name,
                                 const std::string &args,
                                 std::function<void(std::string)> callback) {
  std::cout << "CallScopedRPCAsync not implemented yet" << std::endl;
}

#ifdef CELTE_SERVER_MODE_ENABLED

void Runtime::MasterInstantiateServerNode(const std::string &payload) {
  throw std::runtime_error("Not implemented");
}

void Runtime::ForceDisconnectClient(const std::string &clientId,
                                    const std::string &payload) {
  // we send the final disconnect message to this grape rpc channel
  CallGrapeExecClientDisconnect()
      .on_scope(RUNTIME.GetAssignedGrape())
      .on_fail_log_error()
      .fire_and_forget(clientId, payload);
}

#else

void Runtime::Disconnect() {
  // we send the disconnect message to all grapes
  for (auto &g : GRAPES.GetGrapes()) {
    CallGrapeRequestClientDisconnect()
        .on_peer(g.second.id)
        .on_fail_log_error()
        .with_timeout(std::chrono::milliseconds(10000))
        .retry(10)
        .fire_and_forget(_uuid);
  }
}
#endif
