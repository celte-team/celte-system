#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "RPCService.hpp"
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

using namespace celte::net;

void caller() {
  RPCService rs({
      .thisPeerUuid = "dummyUUID",
      .listenOn = {"non-persistent://public/default/caller.rpc"},
  });

  int v =
      rs.Call<int>("non-persistent://public/default/callee.rpc", "add", 2, 2);
  std::cout << "sum2p2 blocking: " << v << std::endl;

  rs.CallAsync<int>("non-persistent://public/default/callee.rpc", "add", 2, 2)
      .Then([](int result) {
        std::cout << "sum2p2 async: " << result << std::endl;
      });

  while (true)
    CelteNet::Instance().ExecThens();
}

void callee() {
  std::cout << "mode calleee" << std::endl;
  RPCService rs({
      .thisPeerUuid = "dummyUUID",
      .listenOn = {"non-persistent://public/default/callee.rpc"},
  });
  std::cout << "Registering mode" << std::endl;
  rs.Register("add", std::function([](int a, int b) { return a + b; }));

  while (true)
    CelteNet::Instance().ExecThens();
}

int main(int ac, char **argv) {
  // test async call

  CelteNet::Instance().Connect("pulsar://localhost:6650");

  if (ac == 2)
    caller();
  else
    callee();
  return 0;
}