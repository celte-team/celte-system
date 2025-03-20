#include "ClientRegistry.hpp"
#include "PeerService.hpp"

#include "Runtime.hpp"
#include "Topics.hpp"

using namespace celte;

void ClientRegistry::RegisterClient(const std::string &clientId,
                                    const std::string &grapeId,
                                    bool isLocallyOwned) {
  _clients.insert({clientId, ClientData{.id = clientId,
                                        .lastSeen = CLOCK.GetUnifiedTime(),
                                        .isLocallyOwned = isLocallyOwned,
                                        .currentOwnerGrape = grapeId}});
}

ClientRegistry::~ClientRegistry() {
  _keepAliveThreadRunning = false;
  if (_keepAliveThread.joinable()) {
    _keepAliveThread.join();
  }
}

void ClientRegistry::StartKeepAliveThread() {
  _keepAliveThreadRunning = true;
  int step = std::atoi(
      RUNTIME.GetConfig().Get("keepAliveThreadStep").value_or("5").c_str());
  _keepAliveThread = std::thread([this, step] {
    while (_keepAliveThreadRunning) {
      std::this_thread::sleep_for(std::chrono::seconds(step));

      tbb::concurrent_hash_map<std::string, ClientData>::accessor accessor;
      for (auto it = _clients.begin(); it != _clients.end(); ++it) {
        RUNTIME.ScheduleAsyncIOTask([this, it] {
          // RUNTIME.GetPeerService().GetRPCService().CallWithTimeout<bool>(
          //     tp::rpc(it->first), "__rp_ping",
          //     std::chrono::milliseconds(
          //         std::atoi(RUNTIME.GetConfig()
          //                       .Get("client_timeout_ms")
          //                       .value_or("2000")
          //                       .c_str())),
          //     true);
          std::string clientId = it->first;
          CallPeerServicePing()
              .on_peer(clientId)
              // .on_fail_log_error()
              .on_fail_do([clientId](auto &e) {
                RUNTIME.Hooks().onClientNotSeen(clientId);
              })
              .with_timeout(std::chrono::milliseconds(
                  std::atoi(RUNTIME.GetConfig()
                                .Get("client_timeout_ms")
                                .value_or("2000")
                                .c_str())))
              .retry(3)
              .call<bool>()
              .value_or(false);
        });
      }
    }
  });
}

void ClientRegistry::ForgetClient(const std::string &clientId) {
  _clients.erase(clientId);
}

void ClientRegistry::DisconnectClient(const std::string &clientId) {
  std::cout << "NOT IMPLEMENTED" << std::endl;
}
