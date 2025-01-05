#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include <iostream>

using namespace celte;

// wraps the entity and its position in the engine
struct CEntity {
  std::shared_ptr<celte::CelteEntity> entity;
  glm::vec3 position;

  inline glm::vec3 getPosition() { return position; }
};

static std::unordered_map<std::string, celte::chunks::Grape *> grapes;
static std::unordered_map<std::string, CEntity> entities;

// loads entities from a list of existing objects
static void loadEntitiesFromSummary(const nlohmann::json &summary) {
  std::string uuid = summary["uuid"];
  if (entities.find(uuid) != entities.end()) {
    return;
  }
  entities[uuid] =
      CEntity{std::make_shared<celte::CelteEntity>(), glm::vec3(0, 0, 0)};
  entities[uuid].entity->OnSpawn(0, 0, 0, uuid);

  std::cout << "Entity " << uuid << " loaded from summary" << std::endl;
}

// initializes the replication graph for a grape
static void initReplGraph(const std::string &grapeId) {
  auto ARN = [](CelteEntity &entity,
                std::shared_ptr<IEntityContainer> container) { return 1; };

  auto SARN = [](void *pentity, std::vector<void *> pgrapes) {
    return "LeChateauDuMechant";
  };

  auto IRN = [](nlohmann::json) { return true; };

#ifdef CELTE_SERVER_MODE_ENABLED
  grapes[grapeId]->GetReplicationGraph().SetAssignmentReplNode(ARN);
  grapes[grapeId]->GetReplicationGraph().SetServerAssignmentReplNode(SARN);
#endif
  grapes[grapeId]->GetReplicationGraph().SetInterestReplNode(IRN);
  grapes[grapeId]->GetReplicationGraph().Validate();
}

// loads a grape into the game
void loadGrape(std::string grapeId, bool isLocallyOwned, glm::vec3 position) {
  auto then = [grapeId, isLocallyOwned]() {
    std::cout << "Grape " << grapeId << " loaded" << std::endl;
    initReplGraph(grapeId);

// ! warning this is in client mode
#ifndef CELTE_SERVER_MODE_ENABLED
    if (not isLocallyOwned and grapeId == "LeChateauDuMechant") {
      RUNTIME.RequestSpawn(RUNTIME.GetUUID(), grapeId, nlohmann::json().dump());
    }
#else // ! warning this is in server mode
    std::cout << ">> SERVER READY <<" << std::endl;
#endif
  };
  celte::chunks::GrapeOptions options = {.grapeId = grapeId,
                                         .subdivision = 1,
                                         .size = glm::vec3(10, 10, 10),
                                         .localX = glm::vec3(1, 0, 0),
                                         .localY = glm::vec3(0, 1, 0),
                                         .localZ = glm::vec3(0, 0, 1),
                                         .position = position,
                                         .isLocallyOwned = isLocallyOwned,
                                         .then = then};
  grapes[grapeId] = &(RUNTIME.GetGrapeManager().RegisterGrape(options));
  grapes[grapeId]->SetEngineWrapperInstancePtr(grapes[grapeId]);
  grapes[grapeId]->SetEntityPositionGetter(
      [](const std::string &entityId) -> glm::vec3 {
        return entities[entityId].getPosition();
      });
  grapes[grapeId]->Initialize();
}

// registers the hooks for the server, call before starting the runtime
static void registerHooks() {
#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.grape.loadGrape = [](std::string grapeId, bool isLocallyOwned) {
#else
  HOOKS.client.grape.loadGrape = [](std::string grapeId) {
    bool isLocallyOwned = false;
#endif
    std::cout << ">> LOAD GRAPE HOOK CALLED <<" << std::endl;
    if (grapeId == "LeChateauDuMechant") {
      std::cout << "Loading LeChateauDuMechant as owned" << std::endl;
      loadGrape(grapeId, isLocallyOwned, glm::vec3(0, 0, 0));
      loadGrape("LeChateauDuGentil", false, glm::vec3(10, 0, 0));
    } else if (grapeId == "LeChateauDuGentil") {
      std::cout << "Loading LeChateauDuGentil as owned" << std::endl;
      loadGrape(grapeId, isLocallyOwned, glm::vec3(10, 0, 0));
      loadGrape("LeChateauDuMechant", false, glm::vec3(0, 0, 0));
    }
    return true;
  };

#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.newPlayerConnected.execPlayerSpawn =
      [](std::string clientId, std::string grapeId, int x, int y, int z) {
#else
  HOOKS.client.player.execPlayerSpawn =
      [](std::string clientId, std::string grapeId, int x, int y, int z) {
#endif
        std::cout << ">> SPAWN " << clientId << " <<" << std::endl;
        entities[clientId] =
            CEntity{std::make_shared<celte::CelteEntity>(), glm::vec3(x, y, z)};
        ENTITIES.RegisterEntity(entities[clientId].entity);
        entities[clientId].entity->OnSpawn(x, y, z, clientId);
        std::cout << "Entity " << clientId << " spawned" << std::endl;
        return true;
      };

#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.connection.onSpawnPositionRequest = [](std::string clientId) {
    std::cout << ">> SERVER onSpawnPositionRequest HOOK CALLED <<" << std::endl;
    return std::make_tuple(clientId, "LeChateauDuMechant", 0, 0, 0);
  };
#endif

#ifdef CELTE_SERVER_MODE_ENABLED
  HOOKS.server.grape.onLoadExistingEntities = [](nlohmann::json summary) {
#else
  HOOKS.client.grape.onLoadExistingEntities = [](nlohmann::json summary) {
#endif
    std::cout << ">> SERVER LOADING EXISTING ENTITIES <<" << std::endl;
    loadEntitiesFromSummary(summary);
    return true;
  };
}

static std::future<void> nextAction;
struct Action {
  int delayMs = 0;
  std::function<void()> work;
};
static auto actionList = std::vector<Action>{
#ifdef CELTE_SERVER_MODE_ENABLED
    // server spawns an object on the network after 5 seconds
    {
        .delayMs = 5000,
        .work =
            []() {
              std::cout << "Spawning object 'object.A'" << std::endl;
              std::string payload("{'type':'object','name':'object.A'}");
              grapes["LeChateauDuMechant"]->SpawnEntity(payload, 0, 0, 0,
                                                        "object.A");
            },
    },
#else
    // client disconnects after 10 seconds
    {
        .delayMs = 10000,
        .work =
            []() {
              std::cout << "Disconnecting from server" << std::endl;
              exit(0);
            },
    },
#endif
};

static void advanceActions() {
  if (nextAction.valid() and nextAction.wait_for(std::chrono::seconds(0)) !=
                                 std::future_status::ready) {
    return;
  }
  if (actionList.empty()) {
    return;
  }
  auto action = actionList.front();
  actionList.erase(actionList.begin());
  nextAction = std::async(std::launch::async, [action]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(action.delayMs));
    action.work();
  });
}

// the game runs here, and runtime ticks are called
#ifdef CELTE_SERVER_MODE_ENABLED

static void doGameLoopServer() {
  RUNTIME.Tick();
  for (auto &[uuid, entity] : entities) {
    entity.entity->Tick();
  }
  for (auto &[_, grape] : grapes) {
    grape->Tick();
  }
  advanceActions();
}
#else
static void doGameLoopClient() {
  RUNTIME.Tick();
  for (auto &[uuid, entity] : entities) {
    entity.entity->Tick();
  }
  for (auto &[_, grape] : grapes) {
    grape->Tick();
  }
  advanceActions();
}
#endif

int main() {
  registerHooks();
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
  RUNTIME.ConnectToCluster("localhost", 6650);

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
#ifdef CELTE_SERVER_MODE_ENABLED
    doGameLoopServer();
#else
    doGameLoopClient();
#endif
  }
  return 0;
}
