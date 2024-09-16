#include "CelteGrapeManagementSystem.hpp"
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
  REGISTER_RPC(__rp_acceptNewClient, celte::rpc::Table::Scope::GRAPPE,
               std::string, std::string, int, int, int);
  REGISTER_RPC(__rp_spawnPlayer, celte::rpc::Table::Scope::GRAPPE, std::string,
               int, int, int);
}

void Connected::__unregisterRPCs() {}

void Connected::__rp_acceptNewClient(std::string clientId, std::string grapeId,
                                     int x, int y, int z) {
  // TODO: add client to correct chunk's authority
  HOOKS.server.newPlayerConnected.accept(clientId);
  try {
    auto &chunk = GRAPES.GetGrape(grapeId).GetChunkByPosition(x, y, z);
    chunk.TakeAuthority(clientId);
  } catch (const std::out_of_range &e) {
    std::cerr << "Error in __rp_acceptNewClient: " << e.what() << std::endl;
  }

  //   TODO: send chunk id and position to client via its id
  //   RUNTIME.KPool().Send()
}

void Connected::__rp_spawnPlayer(std::string clientId, int x, int y, int z) {
  HOOKS.server.newPlayerConnected.spawnPlayer(clientId, x, y, z);
}

} // namespace states
} // namespace server
} // namespace celte