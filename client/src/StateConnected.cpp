#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"

namespace celte {
namespace client {
namespace states {
void Connected::entry() { std::cerr << "Entering StateConnected" << std::endl; }

void Connected::exit() { std::cerr << "Exiting StateConnected" << std::endl; }

void Connected::react(EDisconnectFromServer const &event) {
  std::cerr << "Disconnecting from server" << std::endl;
  transit<Disconnected>();
}

void Connected::__registerRPCs() {
  std::cerr << "Registering RPCs" << std::endl;
  REGISTER_RPC(__rp_forceConnectToChunk, celte::rpc::Table::Scope::PEER,
               std::string, float, float, float);
  REGISTER_RPC(__rp_spawnPlayer, celte::rpc::Table::Scope::CHUNK, std::string,
               float, float, float);

  // creating a listener for RPCs related to this client as a whole
  KPOOL.Subscribe({
      .topic = RUNTIME.GetUUID() + ".rpc",
      .autoCreateTopic = true,
      .autoPoll = false,
      .callback =
          [this](auto r) {
            std::cout << "INVOKE LOCAL IN CLIENT RPC LISTENER" << std::endl;
            RPC.InvokeLocal(r);
          },
  });
}

void Connected::__rp_forceConnectToChunk(std::string grapeId, float x, float y,
                                         float z) {
  std::cerr << "Forcing connection to grape " << grapeId << " at " << x << ", "
            << y << ", " << z << std::endl;
  // TODO: subscribe to chunk channels and start listening for updates
  std::cout
      << "TODO: subscribe to chunk channels and start listening for updates"
      << std::endl;
  HOOKS.client.connection.onReadyToSpawn(grapeId, x, y, z);
}

void Connected::__rp_spawnPlayer(std::string clientId, float x, float y,
                                 float z) {
  std::cout << "Spawning player " << clientId << " at " << x << ", " << y << " "
            << z << std::endl;
  HOOKS.client.player.execPlayerSpawn(clientId, x, y, z);
}

} // namespace states
} // namespace client
} // namespace celte