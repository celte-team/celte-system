#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

static std::shared_ptr<celte::CelteEntity> entity = nullptr;

void loadGrape(std::string grapeId, bool isLocallyOwned) {
  // Should load eight chunks (2x2x2)
  auto grapeOptions =
      celte::chunks::GrapeOptions{.grapeId = grapeId,
                                  .subdivision = 2,
                                  .position = glm::vec3(0, 0, 0),
                                  .size = glm::vec3(10, 10, 10),
                                  .localX = glm::vec3(1, 0, 0),
                                  .localY = glm::vec3(0, 1, 0),
                                  .localZ = glm::vec3(0, 0, 1)};
  celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
      grapeOptions);

  // Check that there are eight chunks
  if (not(celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
              .GetGrape(grapeId)
              .GetStatistics()
              .numberOfChunks == 8)) {
    throw std::runtime_error("Grape should have 8 chunks");
  }
}

void registerHooks() {
  HOOKS.client.connection.onConnectionProcedureInitiated = []() {
    std::cout << "Connection procedure initiated" << std::endl;
    return true;
  };
  HOOKS.client.connection.onConnectionSuccess = []() {
    std::cout << "Connection procedure success" << std::endl;
    return true;
  };
  HOOKS.client.connection.onReadyToSpawn = [](const std::string &grapeId,
                                              float x, float y, float z) {
    std::cout << "Client is ready to spawn" << std::endl;
    RUNTIME.RequestSpawn(RUNTIME.GetUUID(), grapeId, x, y, z);
    return true;
  };

  HOOKS.client.grape.loadGrape = [](std::string grapeId) {
    std::cout << "Client is loading grape" << std::endl;
    loadGrape(grapeId, false);
    return true;
  };

  HOOKS.client.player.execPlayerSpawn = [](std::string clientId, int x, int y,
                                           int z) {
    std::cout << "Spawning player " << clientId << " at " << x << ", " << y
              << ", " << z << std::endl;
    // Create a new entity
    entity = std::make_shared<celte::CelteEntity>();
    // no information to load because not on server side
    entity->OnSpawn(x, y, z, clientId);

    return true;
  };
}

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::CLIENT);
  RUNTIME.ConnectToCluster("localhost", 80);
  while (true) {
    celte::runtime::CelteRuntime::GetInstance().Tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return 0;
}
