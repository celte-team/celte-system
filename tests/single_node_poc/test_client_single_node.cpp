/*
 * Filename:
 * /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc/test_client_single_node.cpp
 * Path: /Users/eliotjanvier/Documents/eip/celte-systems/tests/single_node_poc
 * Created Date: Tuesday, September 3rd 2024, 12:43:51 pm
 * Author: Eliot Janvier
 *
 * Copyright (c) 2024 Your Company
 *
 * Description:
 * This file uses the runtime to test the client's relationship with the server.
 * Commands are applied manually to simulate the evolution of the game state.
 */

#include "BasicMovementGame.cpp"
#include "CelteRuntime.hpp"
#include <iostream>

// TODO create celte chunk wrapper

int main()
{
    auto runtime = celte::runtime::CelteRuntime::GetInstance();
    runtime.Start(celte::runtime::RuntimeMode::CLIENT);
    runtime.ConnectToCluster("127.0.0.1", 80);

    if (not runtime.IsConnectedToCluster()) {
        throw std::runtime_error("Client should be connected to the cluster");
    }

    dummy::Engine engine;
    // Updating the celte runtime each frame
    // engine.RegisterGameLoopStep([&runtime]() { runtime.Tick(); });
}
