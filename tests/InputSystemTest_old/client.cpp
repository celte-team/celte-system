#include "CelteEntity.hpp"
#include "CelteRuntime.hpp"
#include "Game1.hpp"
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <unistd.h>
static Game game;

static bool Spawned = false;
static std::string id_p = "";
char hash(std::string& s) { return s[7]; }

void loadEntitiesFromSummary(const nlohmann::json& summary)
{
    std::string uuid = summary["uuid"];
    std::string chunk = summary["chunk"];
    std::string info = summary["info"];
    std::string passiveProps = summary["passiveProps"];
    std::string activeProps = summary["activeProps"];

    char repr = std::atoi(info.c_str());

    game.AddObject(uuid, repr, 0, 0);

    // set the current state of the object from the data received from the server
    auto& obj = game.objects[uuid];
    obj->entity->DownloadReplicationData(passiveProps, false);
    obj->entity->DownloadReplicationData(activeProps, true);

    std::cout << "[LOADED ENTITY] " << uuid << " in chunk " << chunk
              << " with info " << info << std::endl;
}

void registerHooks()
{
    HOOKS.client.grape.loadGrape = [](std::string grapeId) {
        game.LoadArea(grapeId, false);
        std::string otherArea;
        if (grapeId == "LeChateauDuMechant") {
            otherArea = "LeChateauDuGentil";
        } else {
            otherArea = "LeChateauDuMechant";
        }
        game.LoadArea(otherArea, false);
        std::cout << ">> CLIENT LOADED MAP << " << std::endl;
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
        char repr = hash(clientId);
        game.AddObject(clientId, repr, x, y);
        game.world.Dump(game.objects);
        Spawned = true;
        id_p = clientId;
        return true;
    };

    HOOKS.client.replication.onActiveReplicationDataReceived =
        [](std::string entityId, std::string blob) {
            std::cout << "Replication data received" << std::endl;
            game.world.Dump(game.objects);
        };
}

void runTestLogic()
{
    static bool status = true;

    if (Spawned) {
        game.objects[id_p]->entity->sendInputToKafka("move forward", status);
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
