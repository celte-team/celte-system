#include "Game1.hpp"
#include <chrono>

static Game game;
static std::unordered_map<std::string, int> clientToPlayerId;
static bool t_isNode1 = false;

char hash(std::string &s) { return s[7]; }

void registerHooks() {
  HOOKS.server.grape.loadGrape = [](std::string grapeId, bool isLocallyOwned) {
    game.LoadArea(grapeId, isLocallyOwned);
    std::string otherArea = "LeChateauDuGentil";
    if (grapeId == "LeChateauDuMechant") {
      t_isNode1 = true;
      std::cout << "THIS IS NODE 1" << std::endl;
      otherArea = "LeChateauDuGentil";
    } else {
      std::cout << "THIS IS NODE 2" << std::endl;
      otherArea = "LeChateauDuMechant";
    }
    game.LoadArea(otherArea, false);
    std::cout << ">> SERVER READY << " << std::endl;
    return true;
  };

  HOOKS.server.newPlayerConnected.execPlayerSpawn = [](std::string clientId,
                                                       int x, int y, int z) {
    std::cout << ">> SERVER SPAWN " << clientId << " <<" << std::endl;
    char repr = hash(clientId);
    game.AddObject(clientId, repr, x, y);
    game.world.Dump(game.objects);
    return true;
  };

  HOOKS.server.connection.onSpawnPositionRequest = [](std::string clientId) {
    std::cout << ">> SERVER onSpawnPositionRequest HOOK CALLED <<" << std::endl;
    clientToPlayerId[clientId] = clientToPlayerId.size();
    return std::make_tuple("LeChateauDuMechant", clientId, 5, 5, 0);
  };

  HOOKS.server.replication.onActiveReplicationDataReceived =
      [](std::string entityId, std::string blob) {
        std::cout << "Replication data received" << std::endl;
        game.world.Dump(game.objects);
      };
}

void updateClientsPositions() {
  static std::chrono::time_point<std::chrono::system_clock> lastUpdate =
      std::chrono::system_clock::now();

  // update the position every 2 seconds
  if (std::chrono::system_clock::now() - lastUpdate < std::chrono::seconds(2)) {
    return;
  }
  lastUpdate = std::chrono::system_clock::now();

  if (not t_isNode1) {
    return;
  }

  if (game.GetNPlayers() == 0) {
    return;
  }

  // get the first player
  auto player = game.objects.begin()->second;
  player->x = (player->x + 1) % game.world.GetXDim();
  game.world.Dump(game.objects);
}

void doGameLoop() {
  RUNTIME.Tick();
  updateClientsPositions();
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