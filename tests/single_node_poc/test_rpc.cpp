
#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include <chrono>

int main() {
  RPC.RegisterAwaitable<int, int>("square", std::function([](int x) -> int {
                                    std::cout << "squaring!" << std::endl;
                                    return x * x;
                                  }));
  RUNTIME.Start(celte::runtime::RuntimeMode::SERVER);
  RUNTIME.ConnectToCluster("127.0.0.1", 80);

  std::cout << "runtime uuid is " << RUNTIME.GetUUID() << std::endl;

  while (not RUNTIME.IsConnectedToCluster()) {
    RUNTIME.Tick();
  }

  std::future<std::string> result = RPC.Call(RUNTIME.GetUUID(), "square", 2);

  auto start = std::chrono::steady_clock::now();
  auto timeout = std::chrono::seconds(5);

  while (true) {
    RUNTIME.Tick();
    if (result.wait_for(std::chrono::milliseconds(100)) ==
        std::future_status::ready) {
      int value = 0;
      celte::rpc::unpack<int>(result.get(), value);
      if (value == 4) {
        std::cout << "Success: RPC call returned correct value." << std::endl;
      } else {
        std::cout << "Error: RPC call returned incorrect value: " << value
                  << std::endl;
        throw std::runtime_error("Error: RPC call returned incorrect value.");
      }
      break;
    }
    if (std::chrono::steady_clock::now() - start > timeout) {
      std::cout << "Error: Timeout exceeded." << std::endl;
      break;
    }
  }
}