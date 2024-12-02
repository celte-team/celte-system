#include "CelteRuntime.hpp"
#include "Game2.hpp"
#include <chrono>

static Game game;
static std::unordered_map<std::string, int> clientToPlayerId;
static bool t_isNode1 = false;

void registerHooks() {
  HOOKS.server.grape.loadGrape = [](std::string grapeId, bool isLocallyOwned) {
    glm::vec3 grapePosition(0, 0, 0);
    auto grapeOptions =
        celte::chunks::GrapeOptions{.grapeId = grapeId,
                                    .subdivision = 1,
                                    .position = grapePosition,
                                    .size = glm::vec3(10, 10, 10),
                                    .localX = glm::vec3(1, 0, 0),
                                    .localY = glm::vec3(0, 1, 0),
                                    .localZ = glm::vec3(0, 0, 1),
                                    .isLocallyOwned = isLocallyOwned,
                                    .then = nullptr};
    celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER().RegisterGrape(
        grapeOptions);
    return true;
  };
}

int main() {
  std::chrono::time_point<std::chrono::system_clock> start =
      std::chrono::system_clock::now();
  int maxTestDurationSeconds = 1000;
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
  RUNTIME.ConnectToCluster("pulsar://localhost", 6650);

  while (std::chrono::system_clock::now() - start <
         std::chrono::seconds(maxTestDurationSeconds)) {
    RUNTIME.Tick();
  }
  return 0;
}