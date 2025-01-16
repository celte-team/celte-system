#pragma once
#ifndef CELTE_SERVER_MODE_ENABLED
#error "Client registry is only available in server mode"
#endif

#include "Clock.hpp"
#include <chrono>
#include <string>
#include <tbb/concurrent_hash_map.h>
#include <thread>

namespace celte {
struct ClientData {
  std::string id;
  std::chrono::time_point<std::chrono::system_clock> lastSeen;
  bool isLocallyOwned;
  std::string currentOwnerGrape;
};

class ClientRegistry {
public:
  ~ClientRegistry();

  /// @brief Registers a client in the registry, meaning that from now on this
  /// client is owned by this server node.
  void RegisterClient(const std::string &clientId, const std::string &grapeId,
                      bool isLocallyOwned);

  /// @brief Unregisters a client from the registry. This server node 'forgets'
  /// about the client but the client might still be connected to the cluster.
  void UnregisterClient(const std::string &clientId);

  /// @brief Disconnects a client from the cluster. This signal will be
  /// broadcasted to all nodes in the cluster.
  void DisconnectClient(const std::string &clientId);

  /// @brief Starts the keep alive thread that will remove clients that have not
  /// been seen for a while. (force disconnect)
  void StartKeepAliveThread();

private:
  tbb::concurrent_hash_map<std::string, ClientData> _clients;
  std::thread _keepAliveThread;
  std::atomic_bool _keepAliveThreadRunning = false;
};
} // namespace celte