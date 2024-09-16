/*
 * Filename:
 * /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc/test_client_single_node.cpp
 * Path: /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc
 * Created Date: Tuesday, September 3rd 2024, 12:43:51 pm
 * Author: Eliot Janvier
 *
 * Copyright (c) 2024 Celte
 *
 * Description:
 * This file uses the runtime to test the client's relationship with the server.
 * Commands are applied manually to simulate the evolution of the game state.
 */

#include "BasicMovementGame.cpp"
#include "CelteGrape.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include <iostream>
#include <string>
#include <thread>

void registerClientHooks() {
  HOOKS.client.connection.onConnectionProcedureInitiated = []() {
    std::cout << "Connection procedure initiated" << std::endl;
    return true;
  };
  HOOKS.client.connection.onConnectionSuccess = []() {
    std::cout << "Connection procedure success" << std::endl;
    return true;
  };
  HOOKS.client.connection.onConnectionError = []() {
    std::cout << "Connection procedure failure" << std::endl;
    return true;
  };
  HOOKS.client.connection.onClientDisconnected = []() {
    std::cout << "Client disconnected" << std::endl;
    return true;
  };
  HOOKS.client.player.onAuthorizeSpawn = [](std::string id, int x, int y,
                                            int z) {
    std::cout << "Client " << id << " is authorized to spawn" << std::endl;
    return true;
  };
}

void registerClientRPC(dummy::Engine &engine) {
  celte::runtime::CelteRuntime::GetInstance().RPCTable().Register(
      "spawnAuthorized",
      std::function<void(std::string)>([&engine](std::string clientId) {
        std::cout << "Client " << clientId << " is authorized to spawn"
                  << std::endl;
        // engine.SpawnPlayer(clientId);
      }));
}

int main(int ac, char **av) {
  // argv 1 is the id of the client
  if (ac < 2) {
    throw std::runtime_error("Client id is required");
  }

  std::string clientId = av[1];

  // waiting for the server to start
  std::this_thread::sleep_for(std::chrono::seconds(1));

  registerClientHooks();

  auto &runtime = celte::runtime::CelteRuntime::GetInstance();
  runtime.Start(celte::runtime::RuntimeMode::CLIENT);
  runtime.ConnectToCluster("127.0.0.1", 80);

  // wait 10 ms for the connection to be established, and a uuid to be available
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // waiting for the client to be connected to the cluster, the Tick() call
  // would be done in the game loop
  std::cout << "now waiting for connection" << std::endl;
  while (not runtime.IsConnectedToCluster()) {
    // throw std::runtime_error("Client should be connected to the cluster");
    runtime.Tick();
  }

#include "COMMON_SETUP.cpp"

  dummy::Engine engine;
  registerClientRPC(engine);

  runtime.Start(celte::runtime::RuntimeMode::SERVER);

  runtime.RequestSpawn(clientId);

  // Updating the celte runtime each frame
  engine.RegisterGameLoopStep([&runtime](float deltaTime) { runtime.Tick(); });
}
