#include "CelteGrapeManagementSystem.hpp"
#include "CelteRuntime.hpp"
#include "Requests.hpp"
#include "ServerStatesDeclaration.hpp"
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
      "__rp_assignGrape", std::function([this](std::string grapeId) {
        try {
          __rp_assignGrape(grapeId);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_assignGrape: " << e.what() << std::endl;
          return false;
        }
      }));

  // rpcs.Register<bool>(
  //     "__rp_loadExistingEntities",
  //     std::function([this](std::string grapeId, std::string summary) {
  //       try {
  //         __rp_loadExistingEntities(grapeId, summary);
  //         return true;
  //       } catch (std::exception &e) {
  //         std::cerr << "Error in __rp_loadExistingEntities: " << e.what()
  //                   << std::endl;
  //         return false;
  //       }
  //     }));

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

// void Connected::__rp_loadExistingEntities(std::string grapeId,
//                                           std::string summary) {
//   ENTITIES.LoadExistingEntities(grapeId, summary);
// }

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
  HOOKS.server.grape.loadGrape(grapeId, true);
}

void Connected::__rp_acceptNewClient(std::string clientId, std::string grapeId,
                                     float x, float y, float z) {
  // TODO: add client to correct chunk's authority
  HOOKS.server.newPlayerConnected.accept(clientId);
  ENTITIES.AddPendingSpawn(clientId, grapeId, x, y, z);
  ServerNet().ConnectClientToGrape(clientId, grapeId, x, y, z);
}

void Connected::__unregisterGrapeConsumers() {}

} // namespace states
} // namespace server
} // namespace celte
