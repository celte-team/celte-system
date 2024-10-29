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
  HOOKS.server.connection.onConnectionProcedureInitiated = []() {
    std::cout << "Connection procedure initiated" << std::endl;
    return true;
  };
  HOOKS.server.connection.onConnectionSuccess = []() {
    std::cout << "Connection procedure success" << std::endl;
    return true;
  };
  HOOKS.server.connection.onSpawnPositionRequest = [](std::string clientId) {
    std::cout << "Master is requesting spawn position" << std::endl;
    return std::make_tuple("LeChateauDuMechant", clientId, 0, 0, 0);
  };
  HOOKS.server.grape.loadGrape = [](std::string grapeId, bool isLocallyOwned) {
    std::cout << "SN is loading grape" << std::endl;
    loadGrape(grapeId, isLocallyOwned);
    return true;
  };
  HOOKS.server.newPlayerConnected.execPlayerSpawn = [](std::string clientId,
                                                       int x, int y, int z) {
    std::cout << "Spawning player " << clientId << " at " << x << ", " << y
              << ", " << z << std::endl;
    // Create a new entity
    entity = std::make_shared<celte::CelteEntity>();
    entity->SetInformationToLoad("test");
    entity->OnSpawn(x, y, z, clientId);

    return true;
  };
}

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
  RUNTIME.ConnectToCluster("localhost", 89);

  std::future<void> future = std::async(std::launch::async, []() {
    // wait for 10 seconds and test runtime.isconnectedtocluster
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (RUNTIME.IsConnectedToCluster()) {
      std::cout << "Connected to cluster" << std::endl;
    } else {
      std::cout << "Not connected to cluster" << std::endl;
      KPOOL.ResetConsumers();
    }
  });

  while (true) {
    RUNTIME.Tick();
    future.wait_for(std::chrono::milliseconds(10));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  return 0;
}