/*
** CELTE, 2024
** server-side
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** CelteRuntime.cpp
*/

#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
#include "tinyfsm.hpp"
#include <chrono>
#include <stdexcept>

namespace tinyfsm {
// This registers the initial state of the FSM for
// client or server networking.
#ifdef CELTE_SERVER_MODE_ENABLED
template <> void Fsm<celte::server::AServer>::set_initial_state() {
  Fsm<celte::server::AServer>::current_state_ptr =
      &_state_instance<celte::server::states::Disconnected>::value;
}
#else
template <> void Fsm<celte::client::AClient>::set_initial_state() {
  Fsm<celte::client::AClient>::current_state_ptr =
      &_state_instance<celte::client::states::Disconnected>::value;
}
#endif
} // namespace tinyfsm

namespace celte {
namespace runtime {
// =================================================================================================
// CELTE PUBLIC METHODS
// =================================================================================================
CelteRuntime::CelteRuntime() : _pool(nullptr) {}

CelteRuntime::~CelteRuntime() {}

rpc::Table &CelteRuntime::GetRPC() { return _rpcTable; }

void CelteRuntime::Start(RuntimeMode mode) {
  _mode = mode;
  __initNetworkLayer(mode);
}

void CelteRuntime::Tick() {
  if (_pool) [[likely]] {
    _pool->CatchUp();
  }
  for (auto &callback : _tickCallbacks) {
    // callback.first is the uuid of the callback
    callback.second();
  }
}

int CelteRuntime::RegisterTickCallback(std::function<void()> callback) {
  _tickCallbacks.insert(std::make_pair(++_tickCallbackId, callback));
  return _tickCallbackId;
}

void CelteRuntime::UnregisterTickCallback(int id) {
  // documentation says that no exception is thrown if the key is not found
  _tickCallbacks.erase(id);
}

CelteRuntime &CelteRuntime::GetInstance() {
  static CelteRuntime instance;
  return instance;
}

// =================================================================================================
// CELTE PRIVATE METHODS
// =================================================================================================
#ifdef CELTE_SERVER_MODE_ENABLED
void CelteRuntime::__initServerRPC() {}

void CelteRuntime::__initServer() { __initServerRPC(); }

#endif

#ifndef CELTE_SERVER_MODE_ENABLED

void CelteRuntime::__initClientRPC() {}

void CelteRuntime::__initClient() { __initClientRPC(); }

void CelteRuntime::RequestSpawn(const std::string &clientId) {}

#endif

void CelteRuntime::__initNetworkLayer(RuntimeMode mode) {
  Services::start();

  switch (mode) {
  case SERVER:
#ifdef CELTE_SERVER_MODE_ENABLED
    __initServer();
#endif
    break;
  case CLIENT:
#ifndef CELTE_SERVER_MODE_ENABLED
    __initClient();
#endif
    break;
  default:
    break;
  }
}

void CelteRuntime::ConnectToCluster(const std::string &ip, int port) {
  _pool = std::make_shared<celte::nl::KafkaPool>(celte::nl::KafkaPool::Options{
      .bootstrapServers = ip + std::string(":") + std::to_string(port)});

  // client / server spcific logic will be handled separately in the FSM
  Services::dispatch(celte::EConnectToCluster{
      .ip = ip,
      .port = port,
      .message = std::make_shared<std::string>("hello")});
  std::cout << "CelteRuntime is connected to kafka cluster at " << ip << ":"
            << port << std::endl;
}

bool CelteRuntime::IsConnectedToCluster() {
  if (_pool and not _uuid.empty()) {
    return true;
  }
  return false;
}

bool CelteRuntime::WaitForClusterConnection(int timeoutMs) {
  auto start = std::chrono::high_resolution_clock::now();
  while (true) {
    if (IsConnectedToCluster()) {
      return true;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if (duration.count() > timeoutMs) {
      return false;
    }
  }
}

nl::KafkaPool &CelteRuntime::KPool() {
  if (!_pool) {
    throw std::logic_error("Kafka pool not initialized");
  }
  return *_pool;
}

} // namespace runtime
} // namespace celte
