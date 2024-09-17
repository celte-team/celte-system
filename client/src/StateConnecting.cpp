#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include <kafka/KafkaProducer.h>

namespace celte {
namespace client {
namespace states {

void Connecting::entry() {
  if (not HOOKS.client.connection.onConnectionProcedureInitiated()) {
    std::cerr << "Connection procedure hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
  }

  auto &kfk = RUNTIME.KPool();
  kfk.Subscribe({
      .topic = "UUID",
      .groupId = "UUID",
      .autoCreateTopic = false,
      .extraProps = {{"max.poll.records", "1"},
                     {"auto.offset.reset", "earliest"}},
      .autoPoll = false,
      .callback = [this](auto r) { __onUUIDReceived(r); },
  });
  // waiting a few ms to make sure the subscription is in place
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // polling manually here to avoid polling uuids multiple times
  kfk.Poll("UUID");
}

void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

void Connecting::react(EConnectionSuccess const &event) {
  if (not HOOKS.client.connection.onConnectionSuccess()) {
    std::cerr << "Connection success hook failed" << std::endl;
    HOOKS.client.connection.onConnectionError();
    transit<Disconnected>();
    return;
  }
  transit<Connected>();
}

void Connecting::__onUUIDReceived(
    const kafka::clients::consumer::ConsumerRecord &record) {
  try {
    std::string uuid(static_cast<const char *>(record.value().data()),
                     record.value().size());
    RUNTIME.SetUUID(uuid);
    KPOOL.Send({
        .topic = "master.hello.client",
        .value = uuid,
        .onDeliveryError =
            [this](auto metadata, auto error) {
              __onHelloDelivered(metadata, error);
            },
    });
  } catch (const std::exception &e) {
    HOOKS.client.connection.onConnectionError();
    HOOKS.client.connection.onClientDisconnected();
    transit<Disconnected>();
  }
}

void Connecting::__onHelloDelivered(
    const kafka::clients::producer::RecordMetadata &metadata,
    kafka::Error error) {
  if (error) {
    RUNTIME.KPool().Unsubscribe("UUID", "UUID", true);
    HOOKS.client.connection.onConnectionError();
    HOOKS.client.connection.onClientDisconnected();
    transit<Disconnected>();
    return;
  }
  RUNTIME.KPool().Unsubscribe("UUID", "UUID", true);
  dispatch(EConnectionSuccess());
}

} // namespace states
} // namespace client
} // namespace celte