#include "Game2.hpp"
#include "base64.hpp"
#include <chrono>

static Game game;
static std::unordered_map<std::string, int> clientToPlayerId;
static bool t_isNode1 = false;

char hash(std::string &s) { return s[7]; }

void loadEntitiesFromSummary(const nlohmann::json &summary) {
  std::string uuid = summary["uuid"];
  std::string chunk = summary["chunk"];
  std::string info = summary["info"];
  std::string passiveProps = summary["passiveProps"];
  std::string activeProps = summary["activeProps"];

  char repr = std::atoi(info.c_str());

  game.AddObject(uuid, repr, 0, 0);

  // set the current state of the object from the data received from the server
  auto &obj = game.objects[uuid];
  obj->entity->DownloadReplicationData(passiveProps);
  obj->entity->DownloadReplicationData(activeProps);

  std::cout << "[LOADED ENTITY] " << uuid << " in chunk " << chunk
            << " with info " << info << std::endl;
}

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
    return std::make_tuple(clientId, "LeChateauDuMechant", 5, 5, 0);
  };

  HOOKS.server.replication.onReplicationDataReceived = [](std::string entityId,
                                                          std::string blob) {
    std::cout << "Replication data received" << std::endl;
    game.world.Dump(game.objects);
  };

  HOOKS.server.authority.onTake = [](std::string entityId,
                                     std::string chunkId) {
    std::cout << "Entity " << entityId << " has been assigned to chunk "
              << chunkId << std::endl;
    std::cout << ">> SERVER onTake HOOK CALLED <<" << std::endl;
  };

  HOOKS.server.grape.onLoadExistingEntities = [](nlohmann::json summary) {
    std::cout << ">> SERVER LOADING EXISTING ENTITIES <<" << std::endl;
    loadEntitiesFromSummary(summary);
    return true;
  };
}

void moveEntity2() {
  if (game.objects.size() < 2) {
    return;
  }

  std::shared_ptr<GameObject> obj =
      (*std::next(game.objects.begin(), 1)).second;
  std::shared_ptr<celte::CelteEntity> entity = obj->entity;

  if (entity == nullptr) {
    return;
  }

  std::string uuidP2 = entity->GetUUID();
  auto last_input_P2 = CINPUT.GetInputCircularBuf(uuidP2, "move");

  if (!last_input_P2 || !last_input_P2->back().status) {
    std::cout << "Status 2 : " << false << std::endl;
    return;
  }

  std::cout << "Status 2 : " << last_input_P2->back().status << std::endl;
  int prevY = obj->y;
  obj->y = (obj->y + 1) % game.world.GetYDim();

  // if we cross a border, check for chunk authority change
  if ((prevY < 10 and obj->y >= 10) or (prevY >= 10 and obj->y < 10)) {
    if (not entity->GetOwnerChunk().IsLocallyOwned()) {
      std::cout << "ENTITY NOT LOCALLY OWNED" << std::endl;
      std::cout << "Clock tick: " << CLOCK.CurrentTick() << std::endl;
      std::cout << "entity is owned by chunk"
                << entity->GetOwnerChunk().GetCombinedId() << std::endl;
      return;
    }

    std::cout << "SERVER CHANGING AUTHORITY" << std::endl;
    auto &currChunkByPosition = GRAPES.GetGrapeByPosition(obj->x, obj->y, 0)
                                    .GetChunkByPosition(obj->x, obj->y, 0);
    std::cout << "New owner is " << currChunkByPosition.GetCombinedId()
              << std::endl;
    currChunkByPosition.OnEnterEntity(entity->GetUUID());
  }
}

void updateClientsPositions() {
  static auto inputs = CINPUT.GetListInput();
  static std::chrono::time_point<std::chrono::system_clock> lastUpdate =
      std::chrono::system_clock::now();

  // update the position every 2 seconds
  if (std::chrono::system_clock::now() - lastUpdate < std::chrono::seconds(2)) {
    return;
  }
  lastUpdate = std::chrono::system_clock::now();

  // second entity to be registered will move horizontally and change node
  moveEntity2();

  if (not t_isNode1) {
    return;
  }

  if (game.GetNPlayers() == 0) {
    return;
  }

  std::string uuidP2 =
      (*std::next(game.objects.begin(), 1)).second->entity->GetUUID();
  std::string uuidP1 =
      (*std::next(game.objects.begin(), 0)).second->entity->GetUUID();
  std::cout << "P1 id is : " << uuidP1 << " | P2 id is : " << uuidP2
            << std::endl;

  auto last_input_P1 = CINPUT.GetInputCircularBuf(uuidP1, "move");

  if (last_input_P1 && last_input_P1->back().status) {
    // get the first player
    auto player = game.objects.begin()->second;
    player->x = (player->x + 1) % game.world.GetXDim();
    std::cout << "New player 1 x is : " << player->x << std::endl;
    std::cout << "Status 1 : " << last_input_P1->back().status << std::endl;
  } else {
    std::cout << "Status 1 : " << false << std::endl;
  }
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
    return 1;
  }

  std::cout << "Connected to cluster" << std::endl;
  while (true) {
    doGameLoop();
  }
  return 0;
}
