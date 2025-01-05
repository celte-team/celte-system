#include "CelteRuntime.hpp"
#include "ClientNetService.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Requests.hpp"

using namespace celte::client;

// ClientNetService ////////////////////////////////////////////////

void ClientNetService::Connect() {
  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = {tp::PERSIST_DEFAULT + RUNTIME.GetUUID() + "." + tp::RPCs},
      .responseTopic = RUNTIME.GetUUID() + "." + tp::RPCs,
      .serviceName = RUNTIME.GetUUID() + ".peer." + tp::RPCs,
  });

  _rpcs->Register<bool>("__rp_keepConnectionAlive",
                        [this]() { return __rp_keepConnectionAlive(); });

  _writerStreamPool.emplace(
      net::WriterStreamPool::Options{
          .idleTimeout = std::chrono::milliseconds(1000),
      },
      RUNTIME.IO());
}

void ClientNetService::Write(const std::string &topic, const std::string &msg,
                             std::function<void(pulsar::Result)> then) {

  req::BinaryDataPacket packet{.binaryData = msg,
                               .peerUuid = RUNTIME.GetUUID()};
  _writerStreamPool->Write(topic, packet, [then](auto result) {
    if (then)
      NET.PushThen([result, then]() { then(result); });
  });
}

bool ClientNetService::__rp_keepConnectionAlive() { return true; }