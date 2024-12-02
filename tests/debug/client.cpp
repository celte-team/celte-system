#include "CelteRuntime.hpp"
#include <chrono>

int main() {
  std::chrono::time_point<std::chrono::system_clock> start =
      std::chrono::system_clock::now();
  int maxTestDurationSeconds = 1000;
  RUNTIME.Start(celte::runtime::RuntimeMode::CLIENT);
  RUNTIME.ConnectToCluster("pulsar://localhost", 6650);

  while (std::chrono::system_clock::now() - start <
         std::chrono::seconds(maxTestDurationSeconds)) {
    RUNTIME.Tick();
  }
  return 0;
}