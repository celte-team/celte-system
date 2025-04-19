#include "CRPC.hpp"
#include "GhostSystem.hpp"
#include "GrapeRegistry.hpp"
#include "HttpClient.hpp"
#include "Logger.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <laserpants/dotenv/dotenv.h>
#ifdef CELTE_SERVER_MODE_ENABLED
#include "MetricsScrapper.hpp"
#endif

using namespace celte;

static std::string make_uuid() {
  boost::uuids::random_generator gen;
  boost::uuids::uuid id = gen();
#ifdef CELTE_SERVER_MODE_ENABLED
  dotenv::init();
  const char *nodeId = getenv("CELTE_NODE_ID");
  if (nodeId) {
    return std::string(nodeId);
  } else {
    return "sn." + boost::uuids::to_string(id);
  }
#else
  return "cl." + boost::uuids::to_string(id);
#endif
}

Runtime::Runtime() : _uuid(make_uuid()) {}

Runtime &Runtime::GetInstance() {
  static Runtime instance;
  return instance;
}

#ifdef CELTE_SERVER_MODE_ENABLED
bool Runtime::__connectToMaster(const std::string &masterAddress,
                                int masterPort) {
  HttpClient http([this](int statusCode, const std::string &message) {
    _hooks.onConnectionFailed();
  });
  nlohmann::json jsonBody(
      {{"Id", _uuid},
       {"Pid", _config.Get("CELTE_NODE_PID").value_or("unknown-parent")},
       {"Ready", true}});
  std::string strResponse =
      http.Post("http://" + masterAddress + ":" + std::to_string(masterPort) +
                    "/server/connect",
                jsonBody);
  nlohmann::json response;
  try {
    std::cout << "string response is " << strResponse << std::endl;
    response = nlohmann::json::parse(strResponse);
  } catch (const std::exception &e) {
    std::cerr << "Error parsing response from master server: " << e.what()
              << std::endl;
    std::cout << "err 0 " << std::endl;
    return false;
  }
  if (response["message"] != "Node accepted") {
    std::cout << "err 1" << std::endl;
    return false;
  }
  if (response["node"]["payload"].is_null()) {
    std::cout << "err 2" << std::endl;
    return false;
  }
  _hooks.onServerReceivedInitializationPayload(
      response["node"]["payload"].get<std::string>());
  return true;
}

void Runtime::Connect() {
  // get config from env
  std::string host = _config.Get("CELTE_HOST").value_or("localhost");
  std::string port = _config.Get("CELTE_PORT").value_or("6650");
  std::string sessionId = _config.Get("CELTE_SESSION_ID").value_or("default");
  std::string masterHost =
      _config.Get("CELTE_MASTER_HOST").value_or("localhost");
  std::string masterPort = _config.Get("CELTE_MASTER_PORT").value_or("1908");
  _config.SetSessionId(sessionId);

  // connect to the pulsar cluster
  net::CelteNet::Instance().Connect(host + ":" + port);
  RPCCalleeStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().StartListeningForAnswers();

  // create the local services that rely on pulsar
  _peerService = std::make_unique<PeerService>(
      std::function<void(bool)>([this, masterHost, masterPort](bool connected) {
        if (!connected) {
          _hooks.onConnectionFailed();
          return;
        }
        try {
          if (!__connectToMaster(masterHost, std::atoi(masterPort.data()))) {
            _hooks.onConnectionFailed();
          }
        } catch (const std::exception &e) {
          std::cout << "Error connecting to master: " << e.what() << std::endl;
          _hooks.onConnectionFailed();
        }
        METRICS.Start(); // metrics should have been registered by now, in the
                         // engine. (i.e before attempting to connect)
        GHOSTSYSTEM.StartReplicationUploadWorker();
        _hooks.onConnectionSuccess();
      }));
}
#else

// nb: this only connects to the pulsar cluster and creates the local services,
// but does not connect to an actual server node.
void Runtime::Connect(const std::string &celteHost, int port,
                      const std::string &sessionId) {
  _config.SetSessionId(sessionId);
  std::string clusterAddress = celteHost + ":" + std::to_string(port);
  __connectToCluster(clusterAddress);
}

void Runtime::__connectToCluster(const std::string &clusterAddress) {
  net::CelteNet::Instance().Connect(clusterAddress);
  RPCCalleeStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().SetClient(net::CelteNet::Instance().GetClientPtr());
  RPCCallerStub::instance().StartListeningForAnswers();
  _peerService = std::make_unique<PeerService>(
      std::function<void(bool)>([this](bool connected) {
        if (!connected) {
          _hooks.onConnectionFailed();
          return;
        }
        _hooks.onConnectionSuccess();
      }));
}
#endif

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
