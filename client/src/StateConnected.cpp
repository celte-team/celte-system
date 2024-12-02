#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"
#include "nlohmann/json.hpp"

namespace celte {
namespace client {
namespace states {
void Connected::entry() { __registerRPCs(); }

void Connected::exit() { __unregisterRPCs(); }

void Connected::react(EDisconnectFromServer const &event) {
  logs::Logger::getInstance().err() << "Disconnecting from server" << std::endl;
  transit<Disconnected>();
}

void Connected::__registerRPCs() {
  auto &rpcs = ClientNet().rpcs();

  rpcs.Register<bool>(
      "__rp_forceConnectToChunk",
      std::function([this](std::string grapeId, float x, float y, float z) {
        try {
          __rp_forceConnectToChunk(grapeId, x, y, z);
          return true;
        } catch (std::exception &e) {
          std::cerr << "Error in __rp_forceConnectToChunk: " << e.what()
                    << std::endl;
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
}

void Connected::__unregisterRPCs() {}

void Connected::__rp_forceConnectToChunk(std::string grapeId, float x, float y,
                                         float z) {
  logs::Logger::getInstance().info()
      << "Force connect to chunk rp has been called" << std::endl;
  // loading the map will instantiate the chunks, thus subscribing to all the
  // required topics
  HOOKS.client.grape.loadGrape(grapeId);
  // HOOKS.client.connection.onReadyToSpawn(grapeId, x, y, z);
}

void Connected::__rp_loadExistingEntities(std::string grapeId,
                                          std::string summary) {
  ENTITIES.LoadExistingEntities(grapeId, summary);
}

} // namespace states
} // namespace client
} // namespace celte
