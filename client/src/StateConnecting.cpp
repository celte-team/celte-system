#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"
#include "topics.hpp"
#include <kafka/KafkaProducer.h>

namespace celte {
namespace client {
namespace states {

void Connecting::entry() {
  KPOOL.Connect();
  if (not HOOKS.client.connection.onConnectionProcedureInitiated()) {
    logs::Logger::getInstance().err()
        << "Connection procedure hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
  }

  __subscribeToTopics();

  KPOOL.CommitSubscriptions();

  KPOOL.Send({
      .topic = celte::tp::MASTER_HELLO_CLIENT,
      .value = RUNTIME.GetUUID(),
      .onDelivered =
          [this](auto metadata, auto error) {
            if (error) {
              HOOKS.client.connection.onConnectionError();
              HOOKS.client.connection.onClientDisconnected();
              transit<Disconnected>();
            } else {
              dispatch(EConnectionSuccess());
            }
          },
  });
}

void Connecting::exit() {
  logs::Logger::getInstance().err() << "Exiting StateConnecting" << std::endl;
}

void Connecting::react(EConnectionSuccess const &event) {
  if (not HOOKS.client.connection.onConnectionSuccess()) {
    logs::Logger::getInstance().err()
        << "Connection success hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
    return;
  }
  transit<Connected>();
}

void Connecting::__subscribeToTopics() {
  // subscribes to the global clock topic
  RUNTIME.GetClock().Init();

  // creating a listener for RPCs related to this client as a whole
  KPOOL.Subscribe({
      .topics{RUNTIME.GetUUID() + "." + celte::tp::RPCs},
      .autoCreateTopic = true,
      .callbacks{[this](auto r) { RPC.InvokeLocal(r); }},
  });

  // creating a listener for RPCs related to the client as a whole
  KPOOL.Subscribe({.topics{RUNTIME.GetUUID() + "." + celte::tp::RPCs},
                   .autoCreateTopic = true,
                   .callbacks = {[this](auto r) { RPC.InvokeLocal(r); }}});
}
} // namespace states
} // namespace client
} // namespace celte
