#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "RPCService.hpp"
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

using namespace celte::net;

// keep the comment below for reference until we have documentation

// struct HelloReq : public celte::net::CelteRequest<HelloReq> {
//   int id;
//   std::string message;
//   std::map<std::string, std::string> headers{
//       {"test-header", "test-header-value"}}; // custom headers

//   void to_json(nlohmann::json &j) const {
//     j = nlohmann::json{{"id", id}, {"message", message}};
//   }

//   void from_json(const nlohmann::json &j) {
//     j.at("id").get_to(id);
//     j.at("message").get_to(message);
//   }
// };

// class DummyService : public CelteService {
// public:
//   DummyService() {
//     _createWriterStream<HelloReq>({
//         .topic = "persistent://public/default/hello",
//         .exclusive = false,
//     });
//   }

//   WriterStream &wsHello() {
//     return *_writerStreams["persistent://public/default/hello"];
//   }
// };

// class DummyReaderService : public CelteService {
// public:
//   DummyReaderService() {
//     _createReaderStream<HelloReq>({
//         .thisPeerUuid = "dummyUUID",
//         .topics = {"persistent://public/default/hello"},
//         .subscriptionName = "",
//         .exclusive = false,
//         .messageHandlerSync =
//             [this](const pulsar::Consumer, HelloReq req) {
//               _count++;
//               std::cout << "Received message: " << req.message << std::endl;
//             },
//     });
//   }
//   int _count = 0;
// };

void caller() {
  std::cout << "Mode caller" << std::endl;
  RPCService rs({
      .thisPeerUuid = "dummyUUID",
      .listenOn = "persistent://public/default/caller.rpc",
  });
  std::cout << "Calling mode" << std::endl;

  int v = rs.Call<int>("persistent://public/default/callee.rpc", "add", 2, 2);
  std::cout << "sum2p2 blocking: " << v << std::endl;

  rs.CallAsync<int>("persistent://public/default/callee.rpc", "add", 2, 2)
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
      .listenOn = "persistent://public/default/callee.rpc",
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