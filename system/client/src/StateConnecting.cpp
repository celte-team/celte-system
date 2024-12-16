#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"
#include "topics.hpp"

namespace celte {
namespace client {
namespace states {
ClientNetService &ClientNet() {
  static ClientNetService service;
  return service;
}

void Connecting::entry() {
  // KPOOL.Connect();
  if (not HOOKS.client.connection.onConnectionProcedureInitiated()) {
    logs::Logger::getInstance().err()
        << "Connection procedure hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
  }

  __subscribeToTopics();
}

void Connecting::exit() {
  logs::Logger::getInstance().err() << "Exiting StateConnecting" << std::endl;
}

void Connecting::react(EConnectionSuccess const &event) {
  if (not HOOKS.client.connection.onConnectionSuccess()) {
    logs::Logger::getInstance().err()
        << "Connection success hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
    return;
  }
  transit<Connected>();
}

void Connecting::__subscribeToTopics() {
  // subscribes to the global clock topic
  RUNTIME.GetClock().Init();

  ClientNet().Connect();

  auto &rpcs = ClientNet().rpcs();

  rpcs.Register<bool>(
      "__rp_forceConnectToChunk",
      std::function([this](std::string grapeId, float x, float y, float z) {
        try {
          __rp_forceConnectToChunk(grapeId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_forceConnectToChunk: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  ClientNet().Write(tp::MASTER_HELLO_CLIENT, RUNTIME.GetUUID(),
                    [this](auto result) {
                      if (result != pulsar::Result::ResultOk) {
                        HOOKS.client.connection.onConnectionError();
                        HOOKS.client.connection.onClientDisconnected();
                        transit<Disconnected>();
                      } else {
                        dispatch(EConnectionSuccess());
                      }
                    });
}

void Connecting::__rp_forceConnectToChunk(std::string grapeId, float x, float y,
                                          float z) {
  logs::Logger::getInstance().info()
      << "Force connect to chunk rp has been called" << std::endl;
  // loading the map will instantiate the chunks, thus subscribing to all the
  // required topics
  std::cout << "Force connect to chunk rp has been called" << std::endl;
  HOOKS.client.grape.loadGrape(grapeId);
  // HOOKS.client.connection.onReadyToSpawn(grapeId, x, y, z);
}

} // namespace states
} // namespace client
} // namespace celte
