#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"
#include "topics.hpp"

namespace celte {
namespace server {
namespace states {

ServerNetService &ServerNet() {
  static ServerNetService service;
  return service;
}

void Connecting::entry() {
  if (not HOOKS.server.connection.onConnectionProcedureInitiated()) {
    std::cerr << "Connection procedure hook failed" << std::endl;
    HOOKS.server.connection.onConnectionError();
    transit<Disconnected>();
  }

  RUNTIME.GetClock().Init();

  ServerNet().Connect();
  ServerNet().Write(tp::MASTER_HELLO_SN, RUNTIME.GetUUID(),
                    [this](auto result) {
                      if (result != pulsar::Result::ResultOk) {
                        HOOKS.server.connection.onConnectionError();
                        HOOKS.server.connection.onServerDisconnected();
                        transit<Disconnected>();
                      } else {
                        dispatch(EConnectionSuccess());
                      }
                    });
}

void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

void Connecting::react(EConnectionSuccess const &event) {
  // if this fails we cancel the connection. Maybe the user cancelled the
  // connection or something.
  if (not HOOKS.server.connection.onConnectionSuccess()) {
    std::cerr << "Connection success hook failed" << std::endl;
    HOOKS.server.connection.onConnectionError();
    transit<Disconnected>();
    return;
  }
  transit<Connected>();
}

} // namespace states
} // namespace server
} // namespace celte
