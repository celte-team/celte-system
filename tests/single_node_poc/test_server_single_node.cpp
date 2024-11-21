/*
 * Filename:
 * /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc/test_server_single_node.cpp
 * Path: /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc
 * Created Date: Tuesday, September 3rd 2024, 12:43:44 pm
 * Author: Eliot Janvier
 *
 * Copyright (c) 2024 Your Company
 *
 * Description:
 *
 * This file uses the runtime to test the server's relationship with the client.
 * Commands are applied manually to simulate the evolution of the game state.
 */

#include "BasicMovementGame.cpp"
#include "CelteGrape.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include <iostream>

void registerServerHooks() {
  HOOKS.server.connection.onConnectionProcedureInitiated = []() {
    std::cout << "Connection procedure initiated" << std::endl;
    return true;
  };
  HOOKS.server.connection.onConnectionSuccess = []() {
    std::cout << "Connection procedure success" << std::endl;
    return true;
  };
  HOOKS.server.connection.onConnectionError = []() {
    std::cout << "Connection procedure failure" << std::endl;
    return true;
  };
  HOOKS.server.connection.onServerDisconnected = []() {
    std::cout << "Client disconnected" << std::endl;
    return true;
  };
  HOOKS.server.connection.onSpawnPositionRequest = [](std::string clientId) {
    std::cout << "Client is requesting spawn position" << std::endl;
    return std::make_tuple("LeChateauDuMechant", clientId, 0, 0, 0);
  };
  HOOKS.server.grape.loadGrape = [](std::string grapeId, bool isLocallyOwned) {
    celte::chunks::GrapeOptions grapeOptions{.grapeId = "LeChateauDuMechant",
                                             .subdivision = 1,
                                             .position = glm::vec3(0, 0, 0),
                                             .size = glm::vec3(10, 10, 10),
                                             .localX = glm::vec3(1, 0, 0),
                                             .localY = glm::vec3(0, 1, 0),
                                             .localZ = glm::vec3(0, 0, 1),
                                             .isLocallyOwned = isLocallyOwned};

    celte::chunks::Grape &grape =
        celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
            .RegisterGrape(grapeOptions);
    std::cout << ">> Grape loaded << " << std::endl;
    return true;
  };

  HOOKS.server.newPlayerConnected.execPlayerSpawn = [](std::string clientId,
                                                       int x, int y, int z) {
    std::cout << ">> Player spawned << " << std::endl;
    return true;
  };
}

void registerServerRPC(celte::runtime::CelteRuntime &runtime,
                       dummy::Engine &engine) {
  celte::runtime::CelteRuntime::GetInstance().RPCTable().Register(
      "spawnAuthorized", std::function<void(int)>([&engine](int clientId) {
        std::cout << "Client " << clientId << " is authorized to spawn"
                  << std::endl;
        // engine.SpawnPlayer(clientId);
      }),
      celte::rpc::Table::Scope::CHUNK);
}

void authorizeSpawn(celte::runtime::CelteRuntime &runtime, int clientId) {
  auto &chunk = celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
                    .GetGrape("LeChateauDuMechant")
                    .GetChunkByPosition(0, 0, 0);
  std::cout << "chunk id is " << chunk.GetCombinedId() << std::endl;
  celte::runtime::CelteRuntime::GetInstance().RPCTable().InvokeChunk(
      chunk.GetCombinedId(), "spawnAuthorized", clientId);
}

int main(int ac, char **av) {
  std::this_thread::sleep_for(std::chrono::seconds(1));

  registerServerHooks();
  std::string ip = std::getenv("CELTE_CLUSTER_HOST");
  auto &runtime = celte::runtime::CelteRuntime::GetInstance();
  runtime.Start(celte::runtime::RuntimeMode::SERVER);
  runtime.ConnectToCluster(ip, 80);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::cout << "now waiting for connection" << std::endl;
  while (not runtime.IsConnectedToCluster()) {
    runtime.Tick();
  }

  dummy::Engine engine;
  registerServerRPC(runtime, engine);

  // Ctrl+C to stop the server
  // std::cout << "now waiting for connection" << std::endl;
  // while (true) {
  //     runtime.Tick();
  // }
  engine.RegisterGameLoopStep([&runtime](float deltaTime) { runtime.Tick(); });

  engine.Run();
}

// #include <boost/json.hpp>
// int main() {
//   boost::json::array j;

//   for (int i = 0; i < 3; ++i) {
//     try {
//       boost::json::object obj;

//       obj["uuid"] = "uuid" + std::to_string(i);
//       obj["chunk"] = "chunkCombinedId" + std::to_string(i);
//       obj["info"] = "info" + std::to_string(i);

//       // Add the object to the JSON array
//       j.push_back(obj);
//     } catch (std::out_of_range &e) {
//     }
//     std::cout << "packing entity " << i << " to json." << std::endl;
//   }

//   // Serialize the JSON array
//   std::string json_str = boost::json::serialize(j);
//   std::cout << json_str << std::endl;
//   return 0;
// }
