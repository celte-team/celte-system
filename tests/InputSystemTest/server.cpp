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
        property = 0;
        entity->RegisterActiveProperty("property", &property);
        std::cout << "pointer to property is " << &property << std::endl;

        entitySpawnTimePoint = std::chrono::system_clock::now();

        return true;
    };
}

void runTestLogic()
{
    static bool one = true;
    auto inputs = CINPUT.getListInput();

    if (!inputs->empty() && one) {
        // Access the first element in the outer map (LIST_INPUTS)
        auto firstData = inputs->begin()->second.begin()->second.front();
        auto firstData2 = CINPUT.getListInputOfUuid(inputs->begin()->first)->begin()->second.front();
        auto firstData3 = CINPUT.getInputCircularBuf(inputs->begin()->first, "move forward")->front();
        auto firstData4 = CINPUT.getSpecificInput(inputs->begin()->first, "move forward", 0);

        if (firstData.status == firstData2.status == firstData3.status == firstData4->status) {
            std::cout << "First element status: " << firstData.status << std::endl;
            std::cout << "First element timestamp: " << std::chrono::system_clock::to_time_t(firstData.timestamp) << std::endl;
        } else {
            std::cout << "Error in get data :\n";

            std::cout << "First element status: " << firstData.status << std::endl;
            std::cout << "First element timestamp: " << std::chrono::system_clock::to_time_t(firstData.timestamp) << std::endl;

            std::cout << "First element status: " << firstData2.status << std::endl;
            std::cout << "First element timestamp: " << std::chrono::system_clock::to_time_t(firstData2.timestamp) << std::endl;

            std::cout << "First element status: " << firstData3.status << std::endl;
            std::cout << "First element timestamp: " << std::chrono::system_clock::to_time_t(firstData3.timestamp) << std::endl;

            std::cout << "First element status: " << firstData4->status << std::endl;
            std::cout << "First element timestamp: " << std::chrono::system_clock::to_time_t(firstData4->timestamp) << std::endl;
        }
        one = false;
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
