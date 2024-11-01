#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

static std::shared_ptr<celte::CelteEntity> entity = nullptr;
static std::chrono::seconds chunkChangeTimer = std::chrono::seconds(5);
static std::chrono::time_point<std::chrono::system_clock> entitySpawnTimePoint =
    std::chrono::system_clock::now();
std::atomic_bool chunkChangeTriggered = false;
static std::chrono::seconds xValueChangeTimer = std::chrono::seconds(10);
std::atomic_bool xValueChangeTriggered = false;
static float x = 0;

void loadGrape(std::string grapeId, bool isLocallyOwned) {
  // Should load eight chunks (2x2x2)
  auto grapeOptions =
      celte::chunks::GrapeOptions{.grapeId = grapeId,
                                  .subdivision = 2,
                                  .position = glm::vec3(0, 0, 0),
                                  .size = glm::vec3(10, 10, 10),
                                  .localX = glm::vec3(1, 0, 0),
                                  .localY = glm::vec3(0, 1, 0),
                                  .localZ = glm::vec3(0, 0, 1),
                                  .isLocallyOwned = isLocallyOwned};
  celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
      grapeOptions);
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
    entity->RegisterProperty("x", x);

    entitySpawnTimePoint = std::chrono::system_clock::now();

    return true;
  };
}

void triggerChunkChange() {
  std::cout << "Triggering chunk change" << std::endl;
  chunkChangeTriggered = true;
  auto &chunk =
      GRAPES.GetGrape("LeChateauDuMechant").GetChunkByPosition(-4, -4, -4);
  std::cout << "transfering authority to chunk " << chunk.GetCombinedId()
            << std::endl;
  chunk.OnEnterEntity(entity->GetUUID());
  std::cout << "after transfering authority" << std::endl;
}

void run_test_logic() {
  if (entity != nullptr) {
    if (std::chrono::system_clock::now() - entitySpawnTimePoint >
            chunkChangeTimer and
        not chunkChangeTriggered) {
      triggerChunkChange();
    }
    if (std::chrono::system_clock::now() - entitySpawnTimePoint >
            xValueChangeTimer and
        not xValueChangeTriggered) {
      x += 1;
      entity->NotifyDataChanged("x");
      std::cout << "Notified data changed" << std::endl;
      xValueChangeTriggered = true;
    }
  }
}

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
  RUNTIME.ConnectToCluster("localhost", 80);

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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    run_test_logic();
  }

  return 0;
}