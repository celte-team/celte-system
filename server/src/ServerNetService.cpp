#include "CelteRequest.hpp"
#include "CelteRuntime.hpp"
#include "Requests.hpp"
#include "ServerNetService.hpp"
#include "ServerStatesDeclaration.hpp"

using namespace celte::server;

// ServerNetService ////////////////////////////////////////////////

void ServerNetService::Connect() {
  _rpcs.emplace(net::RPCService::Options{
      .thisPeerUuid = RUNTIME.GetUUID(),
      .listenOn = {tp::PERSIST_DEFAULT + RUNTIME.GetUUID() + "." + tp::RPCs},
      .reponseTopic = RUNTIME.GetUUID(),
      .serviceName = RUNTIME.GetUUID() + ".peer." + tp::RPCs,
  });

  __init(); // init available rpcs

  _writerStreamPool.emplace(
      net::WriterStreamPool::Options{
          .idleTimeout = std::chrono::milliseconds(1000),
      },
      RUNTIME.IO());
}

void ServerNetService::Write(const std::string &topic, const std::string &msg,
                             std::function<void(pulsar::Result)> then) {

  req::BinaryDataPacket packet{.binaryData = msg,
                               .peerUuid = RUNTIME.GetUUID()};
  _writerStreamPool->Write(topic, packet, [then](auto result) {
    if (then)
      NET.PushThen([result, then]() { then(result); });
  });
}

void ServerNetService::__init() {}