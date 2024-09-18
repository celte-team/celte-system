#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"

namespace celte {
namespace server {
namespace states {
void Connecting::entry() {
  if (not HOOKS.server.connection.onConnectionProcedureInitiated()) {
    std::cerr << "Connection procedure hook failed" << std::endl;
    HOOKS.server.connection.onConnectionError();
    transit<Disconnected>();
  }

  KPOOL.Send({
      .topic = "master.hello.sn",
      .value = runtime::PEER_UUID,
      .onDelivered =
          [this](auto metadata, auto error) {
            if (error) {
              HOOKS.server.connection.onConnectionError();
              HOOKS.server.connection.onServerDisconnected();
              transit<Disconnected>();
            } else {
              dispatch(EConnectionSuccess());
            }
          },
  });
}

void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

void Connecting::react(EConnectionSuccess const &event) {
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
