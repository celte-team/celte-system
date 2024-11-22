#include "Game1.hpp"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
static Game game;

char hash(std::string &s) { return s[7]; }

void loadEntitiesFromSummary(const nlohmann::json &summary) {
  /*
  Format is :
  [
  {
    "uuid": "uuid",
    "chunk": "chunkCombinedId",
    "info": "info"
  },
  {
    "uuid": "uuid",
    "chunk": "chunkCombinedId",
    "info": "info"
  }
  ]
  */
  for (const auto &entityData : summary) {
    std::string uuid = entityData["uuid"];
    std::string chunk = entityData["chunk"];
    std::string info = entityData["info"];
    std::cout << "[FROM SUMMARY] loading entity " << uuid << " in chunk "
              << chunk << " with info " << info << std::endl;
    game.AddObject(uuid, info[0], 0, 0);
  }
  std::cout << "client loaded entities, resuming" << std::endl;
}

void registerHooks() {
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

  HOOKS.client.connection.onReadyToSpawn = [](const std::string &grapeId,
                                              float x, float y, float z) {
    std::cout << ">> CLIENT IS READY TO SPAWN <<" << std::endl;
    RUNTIME.RequestSpawn(RUNTIME.GetUUID(), grapeId, x, y, z);
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
    char repr = hash(clientId);
    game.AddObject(clientId, repr, x, y);
    game.world.Dump(game.objects);
    return true;
  };

  HOOKS.client.replication.onActiveReplicationDataReceived =
      [](std::string entityId, std::string blob) {
        std::cout << "Replication data received" << std::endl;
        game.world.Dump(game.objects);
      };
}

void printMap() {
  static std::chrono::time_point<std::chrono::system_clock> lastUpdate =
      std::chrono::system_clock::now();

  if (game.GetNPlayers() == 0) {
    return;
  }

  // update the position every 2 seconds
  if (std::chrono::system_clock::now() - lastUpdate < std::chrono::seconds(2)) {
    return;
  }
  lastUpdate = std::chrono::system_clock::now();
}

void doGameLoop() {
  RUNTIME.Tick();
  printMap();
}

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
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
    doGameLoop();
  }
  return 0;
}