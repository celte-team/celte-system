#include "CelteHooks.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include <kafka/KafkaProducer.h>

namespace celte {
namespace client {
namespace states {

void Connecting::entry() {
  // CALL_HOOKS(api::HooksTable::client::connection::onClientConnecting);
  // CALL_HOOKS(client.connection.onConnectionProcedureInitiated);
  RUNTIME.Hooks().Call(
      RUNTIME.Hooks().client.connection.onConnectionProcedureInitiated);
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
  // CALL_HOOKS(api::HooksTable::client::connection::onConnectionSuccess);
  transit<Connected>();
}

void Connecting::__onUUIDReceived(
    const kafka::clients::consumer::ConsumerRecord &record) {
  try {
    std::string uuid(static_cast<const char *>(record.value().data()),
                     record.value().size());
    RUNTIME.SetUUID(uuid);
    std::cerr << "Received UUID: " << uuid << std::endl;

    auto pUuid = std::shared_ptr<std::string>(new std::string(uuid));
    const kafka::clients::producer::ProducerRecord record(
        "master.hello.client", kafka::NullKey,
        kafka::Value(pUuid->c_str(), pUuid->size()));

    RUNTIME.KPool().Send(record, [this](auto metadata, auto error) {
      __onHelloDelivered(metadata, error);
    });

    // this will transit all services to Connected
  } catch (const std::exception &e) {
    std::cerr << "Error in Connecting::entry: " << e.what() << std::endl;
    // CALL_HOOKS(api::HooksTable::client::connection::onConnectionError);
    transit<Disconnected>();
  }
}

void Connecting::__onHelloDelivered(
    const kafka::clients::producer::RecordMetadata &metadata,
    kafka::Error error) {
  if (error) {
    std::cerr << "Error delivering hello message" << std::endl;
    RUNTIME.KPool().Unsubscribe("UUID", "UUID", true);
    // CALL_HOOKS(api::HooksTable::client::connection::onConnectionError);
    // CALL_HOOKS(api::HooksTable::client::connection::onClientDisconnected);
    transit<Disconnected>();
    return;
  }
  RUNTIME.KPool().Unsubscribe("UUID", "UUID", true);
  dispatch(EConnectionSuccess());
}

// this will transit all services to Connected

} // namespace states
} // namespace client
} // namespace celte