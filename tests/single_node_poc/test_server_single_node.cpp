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
#include "CelteRuntime.hpp"
#include <iostream>
#include "CelteGrapeManagementSystem.hpp"
#include "CelteGrape.hpp"
#include "CelteRPC.hpp"

int main()
{
    auto runtime = celte::runtime::CelteRuntime::GetInstance();
    runtime.Start(celte::runtime::RuntimeMode::SERVER);
    runtime.ConnectToCluster("127.0.0.1", 80);

    if (not runtime.IsConnectedToCluster()) {
        throw std::runtime_error("Server should be connected to the cluster");
    }

    std::cout << "server is done" << std::endl;
}
