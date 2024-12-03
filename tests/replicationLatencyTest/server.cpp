#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

static std::shared_ptr<celte::CelteEntity> entity = nullptr;
std::chrono::time_point<std::chrono::system_clock> entitySpawnTimePoint = std::chrono::system_clock::now();
static int property = 0;
static bool spawn = false;

void loadGrape()
{
    std::string grapeName("LeChateauDuMechant");
    glm::vec3 grapePosition(0, 0, 0);
    auto grapeOptions = celte::chunks::GrapeOptions { .grapeId = grapeName,
        .subdivision = 1,
        .position = grapePosition,
        .size = glm::vec3(10, 10, 10),
        .localX = glm::vec3(1, 0, 0),
        .localY = glm::vec3(0, 1, 0),
        .localZ = glm::vec3(0, 0, 1),
        .isLocallyOwned = true };
    celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
        grapeOptions);
}

void registerHooks()
{
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
        loadGrape();
        std::cout << ">> Grape has been loaded <<" << std::endl;
        return true;
    };
    HOOKS.server.newPlayerConnected.accept = [](std::string clientId) {
        std::cout << "I'm inside the accept\n";
        return true;
    };
    HOOKS.server.newPlayerConnected.execPlayerSpawn = [](std::string clientId,
                                                          int x, int y, int z) {
        std::cout << ">> Called exec spawn hook <<" << std::endl;
        // Create a new entity
        entity = std::make_shared<celte::CelteEntity>();
        entity->SetInformationToLoad("test");
        entity->OnSpawn(x, y, z, clientId);
        entity->RegisterActiveProperty("property", &property);
        entitySpawnTimePoint = std::chrono::system_clock::now();
        spawn = true;

        return true;
    };
}


void runTestLogic() {
  // if entity has spawned more that 10 seconds ago, change property once.
  if (std::chrono::system_clock::now() - entitySpawnTimePoint >
          std::chrono::seconds(10) and
      // property == 0 and spawn) {
      spawn) {
    std::cout << "property set to one on server side" << std::endl;
    // property = 1;
    property++;
  }
}

int main()
{
    registerHooks();
    RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
    RUNTIME.ConnectToCluster();

    int connectionTimeoutMs = 5000;
    auto connectionTimeout = std::chrono::system_clock::now() + std::chrono::milliseconds(connectionTimeoutMs);
    while (RUNTIME.IsConnectedToCluster() == false and std::chrono::system_clock::now() < connectionTimeout) {
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
