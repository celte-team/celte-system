#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"
#include "topics.hpp"
#include <kafka/KafkaProducer.h>

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
  ClientNet().Write(tp::MASTER_HELLO_CLIENT, RUNTIME.GetUUID(),
                    [this](auto result) {
                      if (result != pulsar::Result::ResultOk) {
                        std::cout << "client connecting failed" << std::endl;
                        HOOKS.client.connection.onConnectionError();
                        HOOKS.client.connection.onClientDisconnected();
                        transit<Disconnected>();
                      } else {
                        std::cout << "client connecting success" << std::endl;
                        dispatch(EConnectionSuccess());
                      }
                    });
}
} // namespace states
} // namespace client
} // namespace celte
