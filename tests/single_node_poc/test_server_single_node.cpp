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
  HOOKS.server.connection.onSpawnPositionRequest = []() {
    std::cout << "Client is requesting spawn position" << std::endl;
    return std::make_tuple("leChateauDuMechant", 0, 0, 0);
  }
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
                    .GetGrape("leChateauDuMechant")
                    .GetChunkByPosition(0, 0, 0);
  std::cout << "chunk id is " << chunk.GetCombinedId() << std::endl;
  celte::runtime::CelteRuntime::GetInstance().RPCTable().InvokeChunk(
      chunk.GetCombinedId(), "spawnAuthorized", clientId);
}

int main(int ac, char **av) {
  std::this_thread::sleep_for(std::chrono::seconds(1));

  registerServerHooks();

  auto &runtime = celte::runtime::CelteRuntime::GetInstance();
  runtime.Start(celte::runtime::RuntimeMode::SERVER);
  runtime.ConnectToCluster("127.0.0.1", 80);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::cout << "now waiting for connection" << std::endl;
  while (not runtime.IsConnectedToCluster()) {
    runtime.Tick();
  }

#include "COMMON_SETUP.cpp"

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
