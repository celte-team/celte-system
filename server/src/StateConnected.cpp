#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"
#include "kafka/KafkaException.h"
#include "topics.hpp"

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
  __unregisterGrapeConsumers();
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
  REGISTER_RPC(__rp_assignGrape, celte::rpc::Table::Scope::GRAPPE, std::string);
  REGISTER_RPC(__rp_sendExistingEntitiesSummary, celte::rpc::Table::Scope::PEER,
               std::string, std::string);

  REGISTER_AWAITABLE_RPC(
      __rp_getPlayerSpawnPosition,
      std::tuple<std::string, std::string, float, float, float>(std::string));
}

void Connected::__rp_sendExistingEntitiesSummary(std::string clientId,
                                                 std::string grapeId) {
  RPC.InvokePeer(clientId, "__rp_loadExistingEntities", grapeId,
                 ENTITIES.GetRegisteredEntitiesSummary());
}

std::tuple<std::string, std::string, float, float, float>
Connected::__rp_getPlayerSpawnPosition(const std::string &clientInfo) {
  return HOOKS.server.connection.onSpawnPositionRequest(clientInfo);
}

void Connected::__unregisterRPCs() {
  UNREGISTER_RPC(__rp_acceptNewClient);
  UNREGISTER_RPC(__rp_onSpawnRequested);
  UNREGISTER_RPC(__rp_spawnPlayer);
  UNREGISTER_RPC(__rp_assignGrape);
  UNREGISTER_RPC(__rp_getPlayerSpawnPosition);
}

void Connected::__rp_assignGrape(std::string grapeId) {
  __registerGrapeConsumers(grapeId);
  // HOOKS.server.grape.loadGrape(grapeId, true);
}

void Connected::__rp_acceptNewClient(std::string clientId, std::string grapeId,
                                     float x, float y, float z) {
  // TODO: add client to correct chunk's authority
  HOOKS.server.newPlayerConnected.accept(clientId);
  RPC.InvokePeer(clientId, "__rp_forceConnectToChunk", grapeId, x, y, z);
  // RPC.InvokePeer(clientId, "__rp_loadExistingEntities", grapeId,
  //                ENTITIES.GetRegisteredEntitiesSummary());
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
  HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
}

void Connected::__registerGrapeConsumers(const std::string &grapeId) {
  try {
    KPOOL.Subscribe(
        {.topics{grapeId + "." + tp::RPCs},
         .autoCreateTopic = true,
         .callbacks{[this](auto r) { RPC.InvokeLocal(r); }},
         .then = [grapeId]() {
           std::cout << "__registerGrapeConsumer.then -> loading the grape"
                     << std::endl;
           HOOKS.server.grape.loadGrape(grapeId, true);
         }});
    KPOOL.CommitSubscriptions();
  } catch (kafka::KafkaException &e) {
    std::cerr << "Error in __registerGrapeConsumers: " << e.what() << std::endl;
  }
}

void Connected::__unregisterGrapeConsumers() {}

} // namespace states
} // namespace server
} // namespace celte
