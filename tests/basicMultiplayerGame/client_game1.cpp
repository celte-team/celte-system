#include "Game1.hpp"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
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

void printMap()
{
    static std::chrono::time_point<std::chrono::system_clock> lastUpdate = std::chrono::system_clock::now();

    if (game.GetNPlayers() == 0) {
        return;
    }

    // update the position every 2 seconds
    if (std::chrono::system_clock::now() - lastUpdate < std::chrono::seconds(2)) {
        return;
    }
    lastUpdate = std::chrono::system_clock::now();
}

void doGameLoop()
{
    static bool status = true;
    static auto savedTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(currentTime - savedTime).count();

    RUNTIME.Tick();
    printMap();
    std::cout << "HEEEEEELLLLLLOOOOOO\n"
              << std::flush;
    std::cout << Spawned << " : " << duration << std::endl
              << std::flush;

    if (Spawned && duration > 5) {
        game.objects.at(id_p)->entity->sendInputToKafka("move", status);
        status = !status;
        savedTime = std::chrono::steady_clock::now();
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
        return 1;
    }

    std::cout << "Connected to cluster" << std::endl;
    while (true) {
        doGameLoop();
    }
    return 0;
}
