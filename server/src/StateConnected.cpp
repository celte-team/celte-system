#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"

namespace celte {
namespace server {
namespace states {
void Connected::entry() { __registerRPCs(); }

void Connected::exit() { __unregisterRPCs(); }

void Connected::react(EDisconnectFromServer const &event) {
  std::cerr << "Disconnecting from server" << std::endl;
  transit<Disconnected>();
}

void Connected::__registerRPCs() {
  //   RUNTIME.RPCTable().Register(
  //       "__rp_acceptNewPlayer",
  //       std::function([this](std::string clientId, int x, int y, int z) {
  //         __rp_acceptNewPlayer(clientId, x, y, z);
  //       }));
  REGISTER_RPC(__rp_acceptNewPlayer, std::string, int, int, int);
}

void Connected::__unregisterRPCs() {}

void Connected::__rp_acceptNewPlayer(std::string clientId, int x, int y,
                                     int z) {
  std::cerr << "Player " << clientId << " connected to the server" << std::endl;
}

} // namespace states
} // namespace server
} // namespace celte