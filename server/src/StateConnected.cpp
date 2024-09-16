#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"

namespace celte {
namespace server {
namespace states {
void Connected::entry() { __registerRPCs(); }

void Connected::exit() { __unregisterRPCs(); }

void Connected::react(EDisconnectFromServer const &event) {
  // this hook is called here and not in Disconnected::entry because do not want
  // to call this at the start of the program. (and the server starts in
  // disconnected state)
  HOOKS.server.connection.onServerDisconnected();
  transit<Disconnected>();
}

void Connected::__registerRPCs() {
  REGISTER_RPC(__rp_acceptNewPlayer, std::string, int, int, int);
}

void Connected::__unregisterRPCs() {}

void Connected::__rp_acceptNewPlayer(std::string clientId, int x, int y,
                                     int z) {
  HOOKS.server.newPlayerConnected.accept(clientId);
  HOOKS.server.newPlayerConnected.spawnPlayer(clientId, x, y, z);
}

} // namespace states
} // namespace server
} // namespace celte