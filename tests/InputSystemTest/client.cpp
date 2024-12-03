#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <unistd.h>

static std::shared_ptr<celte::CelteEntity> entity = nullptr;
static int property = 0;
static bool Spawned = false;

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
        .isLocallyOwned = false };
    celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
        grapeOptions);
}

void loadEntitiesFromSummary(const nlohmann::json& summary)
{
    std::string uuid = summary["uuid"];
    std::string chunk = summary["chunk"];
    std::string info = summary["info"];
    std::string passiveProps = summary["passiveProps"];
    std::string activeProps = summary["activeProps"];

    char repr = std::atoi(info.c_str());

    // set the current state of the object from the data received from the server
    std::cout << "[LOADED ENTITY] " << uuid << " in chunk " << chunk
              << " with info " << info << std::endl;
}

void registerHooks()
{
    HOOKS.client.grape.loadGrape = [](std::string grapeId) {
        std::cout << "Client is loading grape" << std::endl;
        loadGrape();
        std::cout << "Client laoded the grape" << std::endl;

        return true;
    };

    HOOKS.client.connection.onReadyToSpawn = [](const std::string& grapeId,
                                                 float x, float y, float z) {
        // std::cout << ">> CLIENT IS READY TO SPAWN <<" << std::endl;
        // RUNTIME.RequestSpawn(RUNTIME.GetUUID(), grapeId, x, y, z);
        return true;
    };

    HOOKS.client.grape.onLoadExistingEntities = [](std::string grapeId,
                                                    nlohmann::json summary) {
        std::cout << ">> CLIENT LOADING EXISTING ENTITIES <<" << std::endl;
        loadEntitiesFromSummary(summary);
        return true;
    };

    HOOKS.client.player.execPlayerSpawn = [](std::string clientId, int x, int y,
                                              int z) {
        std::cout << ">> CLIENT SPAWNING  " << clientId << " <<" << std::endl;
        std::cout << "spawn coordinates are " << x << " " << y << " " << z
                  << std::endl;
        entity = std::make_shared<celte::CelteEntity>();
        std::cout << ">> Called exec spawn hook <<" << std::endl;
        // no information to load because not on server side
        entity->OnSpawn(x, y, z, clientId);

        entity->RegisterActiveProperty("property", &property);
        Spawned = true;
        return true;
    };

    HOOKS.client.replication.onActiveReplicationDataReceived =
        [](std::string entityId, std::string blob) {
            std::cout << "Replication data received" << std::endl;
        };
}

void runTestLogic()
{
    static bool status = true;

    if (Spawned) {
        entity->sendInputToKafka("move forward", status);
        std::cout << "send move forward\n";
        usleep(5000000);
        status = !status;
    }

    // auto ilist = CINPUT.getListInput();
}

int main()
{
    registerHooks();
    RUNTIME.Start(celte::runtime::RuntimeMode::CLIENT);
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

    printf("FINISH PUTE\n");

    return 0;
}
