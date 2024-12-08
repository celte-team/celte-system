/**
 * Include this file in both server node test and client node test main to setup
 * Celte runtime and grape creation.
 * This reflects real life application since both client and server's
 * binary will have this part in common at compilation.
 */
#pragma region GAME_SETUP
// Simulating the creation of the scene

// Creating a grape as a cube of 10x10x10 centered at (0, 0, 0)
celte::chunks::GrapeOptions grapeOptions{.grapeId = "LeChateauDuMechant",
                                         .subdivision = 1,
                                         .position = glm::vec3(0, 0, 0),
                                         .size = glm::vec3(10, 10, 10),
                                         .localX = glm::vec3(1, 0, 0),
                                         .localY = glm::vec3(0, 1, 0),
                                         .localZ = glm::vec3(0, 0, 1)};

celte::chunks::Grape &grape =
    celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
        grapeOptions);
auto stats = grape.GetStatistics();

{ // DEBUG Displaying grape statistics
  std::cout << "Grape " << stats.grapeId << " has " << stats.numberOfChunks
            << " chunks" << std::endl;
  std::cout << "Chunks ids: " << std::endl;
  for (auto &chunkId : stats.chunksIds) {
    std::cout << "\t- " << chunkId << "\n";
  }
  std::cout << std::endl;
}
#pragma endregion