#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
#include "Requests.hpp"
#include "ServerStatesDeclaration.hpp"
#include "kafka/KafkaException.h"
#include "topics.hpp"

namespace celte {
namespace server {
namespace states {

void Connected::entry() {
  auto &rpcs = ServerNet().rpcs();

  rpcs.Register<bool>(
      "__rp_acceptNewClient",
      std::function([this](std::string clientId, std::string grapeId, float x,
                           float y, float z) {
        try {
          __rp_acceptNewClient(clientId, grapeId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_acceptNewClient: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  rpcs.Register<bool>(
      "__rp_onSpawnRequested",
      std::function([this](std::string clientId, float x, float y, float z) {
        try {
          __rp_onSpawnRequested(clientId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_onSpawnRequested: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  rpcs.Register<bool>(
      "__rp_spawnPlayer",
      std::function([this](std::string clientId, float x, float y, float z) {
        try {
          __rp_spawnPlayer(clientId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_spawnPlayer: " << e.what() << std::endl;
          return false;
        }
      }));

  rpcs.Register<bool>(
      "__rp_assignGrape", std::function([this](std::string grapeId) {
        try {
          __rp_assignGrape(grapeId);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_assignGrape: " << e.what() << std::endl;
          return false;
        }
      }));

  rpcs.Register<bool>(
      "__rp_loadExistingEntities",
      std::function([this](std::string grapeId, std::string summary) {
        try {
          __rp_loadExistingEntities(grapeId, summary);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_loadExistingEntities: " << e.what()
                    << std::endl;
          return false;
        }
      }));

  rpcs.Register<req::SpawnPositionRequest>(
      "__rp_getPlayerSpawnPosition",
      std::function([this](std::string clientInfo) {
        std::tuple<std::string, std::string, float, float, float> spawnInfo =
            HOOKS.server.connection.onSpawnPositionRequest(clientInfo);
        return req::SpawnPositionRequest{
            .clientId = std::get<0>(spawnInfo),
            .grapeId = std::get<1>(spawnInfo),
            .x = std::get<2>(spawnInfo),
            .y = std::get<3>(spawnInfo),
            .z = std::get<4>(spawnInfo),
        };
      }));
}

void Connected::exit() {}

void Connected::react(EDisconnectFromServer const &event) {}

// void Connected::__rp_sendExistingEntitiesSummary(std::string clientId,
//                                                  std::string grapeId) {
//   RPC.InvokePeer(clientId, "__rp_loadExistingEntities", grapeId,
//                  ENTITIES.GetRegisteredEntitiesSummary());
// }

void Connected::__rp_loadExistingEntities(std::string grapeId,
                                          std::string summary) {
  ENTITIES.LoadExistingEntities(grapeId, summary);
}

std::tuple<std::string, std::string, float, float, float>
Connected::__rp_getPlayerSpawnPosition(const std::string &clientInfo) {
  return HOOKS.server.connection.onSpawnPositionRequest(clientInfo);
}

// void Connected::__unregisterRPCs() {
//   UNREGISTER_RPC(__rp_acceptNewClient);
//   UNREGISTER_RPC(__rp_onSpawnRequested);
//   UNREGISTER_RPC(__rp_spawnPlayer);
//   UNREGISTER_RPC(__rp_assignGrape);
//   UNREGISTER_RPC(__rp_getPlayerSpawnPosition);
// }

void Connected::__rp_assignGrape(std::string grapeId) {
  __registerGrapeConsumers(grapeId);
}

void Connected::__rp_acceptNewClient(std::string clientId, std::string grapeId,
                                     float x, float y, float z) {
  // TODO: add client to correct chunk's authority
  HOOKS.server.newPlayerConnected.accept(clientId);
  ENTITIES.AddPendingSpawn(clientId, grapeId, x, y, z);
  // RPC.InvokePeer(clientId, "__rp_forceConnectToChunk", grapeId, x, y, z);
  ServerNet().ConnectClientToGrape(clientId, grapeId, x, y, z);
}

void Connected::__rp_onSpawnRequested(const std::string &clientId, float x,
                                      float y,
                                      float z) { // TODO remove these arguments
  try {
    std::tuple<std::string, float, float, float> spawnInfo =
        ENTITIES.GetPendingSpawn(clientId);
    x = std::get<1>(spawnInfo);
    y = std::get<2>(spawnInfo);
    z = std::get<3>(spawnInfo);
    auto chunkId = GRAPES.GetGrapeByPosition(x, y, z)
                       .GetChunkByPosition(x, y, z)
                       .GetCombinedId();
    RPC.InvokeChunk(chunkId, "__rp_spawnPlayer", clientId, x, y, z);
    ENTITIES.RemovePendingSpawn(clientId);
  } catch (std::out_of_range &e) {
    std::cerr << "Error in __rp_onSpawnRequested: " << e.what() << std::endl;
  }
}

void Connected::__rp_spawnPlayer(std::string clientId, float x, float y,
                                 float z) {
  HOOKS.server.newPlayerConnected.execPlayerSpawn(clientId, x, y, z);
}

void Connected::__registerGrapeConsumers(const std::string &grapeId) {
  std::cout << "__registerGrapeConsumers -> registering consumers for grape "
            << grapeId << std::endl;
  // try {
  // KPOOL.Subscribe(
  //     {.topics{grapeId + "." + tp::RPCs},
  //      .autoCreateTopic = true,
  //      .callbacks{[this](auto r) { RPC.InvokeLocal(r); }},
  //      .then = [grapeId]() {
  //        std::cout << "__registerGrapeConsumer.then -> loading the grape"
  //                  << std::endl;
  //        HOOKS.server.grape.loadGrape(grapeId, true);
  //      }});
  // KPOOL.CommitSubscriptions();
  // } catch (kafka::KafkaException &e) {
  //   std::cerr << "Error in __registerGrapeConsumers: " << e.what() <<
  //   std::endl;
  // }
}

void Connected::__unregisterGrapeConsumers() {}

} // namespace states
} // namespace server
} // namespace celte
