#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

static std::shared_ptr<celte::CelteEntity> entity = nullptr;
static int property = 0;
static bool propertyChanged = false;

void loadGrape() {
  std::string grapeName("LeChateauDuMechant");
  glm::vec3 grapePosition(0, 0, 0);
  auto grapeOptions = celte::chunks::GrapeOptions{.grapeId = grapeName,
                                                  .subdivision = 1,
                                                  .position = grapePosition,
                                                  .size = glm::vec3(10, 10, 10),
                                                  .localX = glm::vec3(1, 0, 0),
                                                  .localY = glm::vec3(0, 1, 0),
                                                  .localZ = glm::vec3(0, 0, 1),
                                                  .isLocallyOwned = false};
  celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
      grapeOptions);
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
    loadGrape();
    std::cout << "Client laoded the grape" << std::endl;
    return true;
  };

  HOOKS.client.player.execPlayerSpawn = [](std::string clientId, int x, int y,
                                           int z) {
    std::cout << "Spawning player " << clientId << " at " << x << ", " << y
              << ", " << z << std::endl;
    // Create a new entity
    entity = std::make_shared<celte::CelteEntity>();
    std::cout << ">> Called exec spawn hook <<" << std::endl;
    // no information to load because not on server side
    entity->OnSpawn(x, y, z, clientId);

    entity->RegisterActiveProperty("property", property);
    return true;
  };
}

void runTestLogic() {
  if (not propertyChanged) {
    if (property != 0) {
      std::cout << ">> property changed to " << property << " <<" << std::endl;
      propertyChanged = true;
      return;
    }
  }
}

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::CLIENT);
  RUNTIME.ConnectToCluster();

  int connectionTimeoutMs = 5000;
  auto connectionTimeout = std::chrono::system_clock::now() +
                           std::chrono::milliseconds(connectionTimeoutMs);
  while (RUNTIME.IsConnectedToCluster() == false and
         std::chrono::system_clock::now() < connectionTimeout) {
    RUNTIME.Tick();
  }
  if (not RUNTIME.IsConnectedToCluster()) {
    std::cout << "Connection failed" << std::endl;
    KPOOL.ResetConsumers();
    return 1;
  }

  std::cout << "Connected to cluster" << std::endl;

  while (true) {
    RUNTIME.Tick();
    runTestLogic();
  }

  return 0;
}
