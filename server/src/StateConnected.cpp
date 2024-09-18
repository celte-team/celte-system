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
               std::string, std::string, float, float, float);
  REGISTER_RPC(__rp_onSpawnRequested, celte::rpc::Table::Scope::GRAPPE,
               std::string, float, float, float);
  // spawns a player in the game world. Clients also have this rpc.
  REGISTER_RPC(__rp_spawnPlayer, celte::rpc::Table::Scope::CHUNK, std::string,
               int, int, int);

  // creating a listener for RPCs related to this server node as a whole
  KPOOL.Subscribe({.topic = runtime::PEER_UUID + ".rpc",
                   .autoCreateTopic = true,
                   .autoPoll = true,
                   .callback = [this](auto r) {
                     std::cout << "INVOKE LOCAL IN SERVER RPC LISTENER"
                               << std::endl;
                     RPC.InvokeLocal(r);
                   }});
}

void Connected::__unregisterRPCs() {
  UNREGISTER_RPC(__rp_acceptNewClient);
  UNREGISTER_RPC(__rp_onSpawnRequested);
  UNREGISTER_RPC(__rp_spawnPlayer);
}

void Connected::__rp_acceptNewClient(std::string clientId, std::string grapeId,
                                     float x, float y, float z) {
  // TODO: add client to correct chunk's authority
  HOOKS.server.newPlayerConnected.accept(clientId);
  try {
    auto &chunk = GRAPES.GetGrape(grapeId).GetChunkByPosition(x, y, z);
    chunk.TakeAuthority(clientId);
  } catch (const std::out_of_range &e) {
    std::cerr << "Error in __rp_acceptNewClient: " << e.what() << std::endl;
  }

  RPC.InvokeByTopic(clientId, "__rp_forceConnectToChunk", grapeId, x, y, z);
}

void Connected::__rp_onSpawnRequested(const std::string &clientId, float x,
                                      float y, float z) {
  // TODO: check if this spawn is legal
  auto chunkId = GRAPES.GetGrapeByPosition(x, y, z)
                     .GetChunkByPosition(x, y, z)
                     .GetCombinedId();
  RPC.InvokeChunk(chunkId, "__rp_spawnPlayer", clientId, x, y, z);
}

void Connected::__rp_spawnPlayer(std::string clientId, float x, float y,
                                 float z) {
  std::cout << "Spawning player " << clientId << " at " << x << ", " << y
            << ", " << z << std::endl;
  HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
}

} // namespace states
} // namespace server
} // namespace celte