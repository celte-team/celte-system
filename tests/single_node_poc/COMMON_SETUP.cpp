/**
 * Include this file in both server node test and client node test main to setup
 * Celte runtime and grape creation.
 * This reflects real life application since both client and server's
 * binary will have this part in common at compilation.
 */
#pragma region GAME_SETUP
    // Simulating the creation of the scene

    // Dummy RPC call to test the RPC system

    celte::rpc::TABLE().RegisterRPC("PlayerShouted", std::function<void(std::string)>([](std::string message) {
        std::cout << "[TEST RPC] Player shouted: " << message << std::endl;
    }));

    // Creating a grape as a cube of 10x10x10 centered at (0, 0, 0)
    celte::chunks::GrapeOptions grapeOptions {
        .grapeId = "leChateauDuMechant",
        .subdivision = 4,
        .position = glm::vec3(0, 0, 0),
        .sizeForward = 10,
        .sizeRight = 10,
        .sizeUp = 10,
        .forward = glm::vec3(0, 0, 1),
        .up = glm::vec3(0, 1, 0)
    };

    celte::chunks::Grape &grape = celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(grapeOptions);
    auto stats = grape.GetStatistics();


    { // DEBUG Displaying grape statistics
        std::cout << "Grape " << stats.grapeId << " has " << stats.numberOfChunks << " chunks" << std::endl;
        std::cout << "Chunks ids: ";
        for (auto& chunkId : stats.chunksIds) {
            std::cout << "\t- " << chunkId << "\n";
        }
        std::cout << std::endl;
    }
#pragma endregion