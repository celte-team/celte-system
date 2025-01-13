#include "PeerService.hpp"
#include "Runtime.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <iostream>

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
  std::cout << "Connecting to pulsar cluster at " << address << ":" << port
            << std::endl;
  net::CelteNet::Instance().Connect(address + ":" + std::to_string(port));
  _peerService = std::make_unique<PeerService>(
      std::function<void(bool)>([this](bool connected) {
        if (!connected) {
          std::cerr << "Error connecting to cluster" << std::endl;
          _hooks.onConnectionFailed();
          return;
        }
        std::cout << "Connected to cluster" << std::endl;
        _hooks.onConnectionSuccess();
      }));
}

void Runtime::Tick() { __advanceSyncTasks(); }

void Runtime::__advanceSyncTasks() {
  std::function<void()> task;
  while (_syncTasks.try_pop(task)) {
    task();
  }
}