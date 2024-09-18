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

static dummy::Engine engine;

void registerClientHooks(const std::string &clientId) {
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

  HOOKS.client.connection.onReadyToSpawn =
      [clientId](const std::string &grapeId, float x, float y, float z) {
        std::cout << "Client is ready to spawn" << std::endl;
        // This could be done later if needed, but not earlier.
        RUNTIME.RequestSpawn(clientId, grapeId, x, y, z);
        return true;
      };

  HOOKS.client.player.execPlayerSpawn = [](std::string clientId, int x, int y,
                                           int z) {
    std::cout << "Spawning player " << clientId << " at " << x << ", " << y
              << ", " << z << std::endl;
    // engine.SpawnPlayer(clientId); ~ or something equivalent
    return true;
  };
}

void registerClientRPC() {
  celte::runtime::CelteRuntime::GetInstance().RPCTable().Register(
      "spawnAuthorized",
      std::function<void(std::string)>([](std::string clientId) {
        std::cout << "Client " << clientId << " is authorized to spawn"
                  << std::endl;
        // engine.SpawnPlayer(clientId);
      }));
}

int main(int ac, char **av) {
  std::string clientId;

  // waiting for the server to start
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Registering the hooks for the client. This would be done by the game dev
  // through api bindings.
  registerClientHooks(clientId);

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
  clientId = celte::runtime::PEER_UUID;

// here we setup the grapes and chunks. This is common between server and
// clients, and would be done by the game dev directly in the engine.
#include "COMMON_SETUP.cpp"

  registerClientRPC();

  // Updating the celte runtime each frame
  engine.RegisterGameLoopStep([&runtime](float deltaTime) { runtime.Tick(); });

  // Running the game loop
  engine.Run();
}
