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
  // argv 1 and 2 are ids of the clients
  if (ac < 3) {
    throw std::runtime_error("Client ids are required");
  }

  int clientId1 = std::atoi(av[1]);
  int clientId2 = std::atoi(av[2]);

  auto &runtime = celte::runtime::CelteRuntime::GetInstance();
  runtime.Start(celte::runtime::RuntimeMode::SERVER);
  runtime.ConnectToCluster("127.0.0.1", 80);

  if (not runtime.WaitForClusterConnection(5000)) {
    throw std::runtime_error("Server should be connected to the cluster");
  }

#include "COMMON_SETUP.cpp"

  dummy::Engine engine;
  registerServerRPC(runtime, engine);

  // This would be done by the master server
  authorizeSpawn(runtime, clientId1);
  authorizeSpawn(runtime, clientId2);

  // Ctrl+C to stop the server
  while (true) {
    runtime.Tick();
  }
}
