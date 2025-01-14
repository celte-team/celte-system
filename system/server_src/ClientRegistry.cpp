#include "ClientRegistry.hpp"
#include "Runtime.hpp"

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
      for (auto &[clientId, clientData] : _clients) {
        if (CLOCK.GetUnifiedTime() - clientData.lastSeen >
            std::chrono::seconds(2 * step)) {
          _clients.erase(clientId);
        }
      }
    }
  });
}

void ClientRegistry::UnregisterClient(const std::string &clientId) {
  _clients.erase(clientId);
}

void ClientRegistry::DisconnectClient(const std::string &clientId) {
  std::cout << "Disconnecting client " << clientId << std::endl;
  std::cout << "NOT IMPLEMENTED" << std::endl;
}
