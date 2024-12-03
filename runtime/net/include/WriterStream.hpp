#pragma once
#include "nlohmann/json.hpp"
#include "pulsar/Producer.h"
#include "pulsar/Schema.h"
#include "queue.hpp"

namespace celte {
namespace net {

/**
 * @brief Handle for an outgoing stream of messages.
 * It uses a pulsar json schema to ensure type safety.
 */
struct WriterStream {
  struct Options {
    std::string topic;
    bool exclusive = false;
    std::function<void(WriterStream &)> onReadySync = nullptr;
    std::function<void()> onConnectErrorSync = nullptr;
    std::function<void(WriterStream &)> onReady = nullptr;
    std::function<void()> onConnectError = nullptr;
  };

  Options options;

  WriterStream(const Options &options) : options(options), _ready(false) {}

  template <typename Req> void Open() {
    auto &net = CelteNet::Instance();
    auto conf = pulsar::ProducerConfiguration();
    conf.setBlockIfQueueFull(true);

    // is access mode exclusive? -> only this producer can write to the
    // topic
    if (options.exclusive)
      conf.setAccessMode(pulsar::ProducerConfiguration::Exclusive);

    // create json schemas from the request type
    // nlohmann::json schemaJson;
    // Req req;
    // to_json(schemaJson, req);
    // std::string schemaStr = schemaJson.dump();
    // pulsar::SchemaInfo schemaInfo(pulsar::SchemaType::JSON, "MessageSchema",
    //                               schemaStr);
    // conf.setSchema(schemaInfo);

    auto pOptions = CelteNet::ProducerOptions{
        .conf = conf,
        .topic = options.topic,
        .then =
            [this](pulsar::Producer producer, const pulsar::Result &result) {
              if (result != pulsar::ResultOk and options.onConnectErrorSync) {
                CelteNet::Instance().PushThen(options.onConnectErrorSync);
                return;
              }
              if (options.onReadySync)
                CelteNet::Instance().PushThen(
                    [this]() { options.onReadySync(*this); });
            },

        .thenAsync =
            [this](pulsar::Producer producer, const pulsar::Result &result) {
              if (result != pulsar::ResultOk and options.onConnectError) {
                options.onConnectError();
                return;
              }
              _producer = producer;
              _ready = true;
              if (options.onReady)
                options.onReady(*this);
            }};
    net.CreateProducer(pOptions);
  }

  template <typename Req>
  void Write(const Req &req,
             std::function<void(pulsar::Result)> onDelivered = nullptr) {
    static_assert(std::is_base_of<CelteRequest<Req>, Req>::value,
                  "Req must inherit from CelteRequest<Req>");
    nlohmann::json j;
    to_json(j, req);

    auto message = pulsar::MessageBuilder().setContent(j.dump());
    for (auto &[key, value] : req.headers) {
      message.setProperty(key, value);
    }

    _pending++;
    _producer.sendAsync(
        message.build(),
        [this, onDelivered](pulsar::Result result,
                            const pulsar::MessageId &messageId) {
          _pending--;
          if (onDelivered)
            onDelivered(result);
        });
  }

  template <typename Req>
  friend WriterStream &operator<<(WriterStream &ws, const Req &req) {
    ws.Write(req);
    return ws;
  }

  bool Ready() { return _ready; }
  bool HasPending() { return _pending > 0; }

private:
  pulsar::Producer _producer;
  std::atomic_bool _ready;
  std::atomic_int _pending{0};
};
} // namespace net
} // namespace celte