#pragma once
#include "Runtime.hpp"
#include "pulsar/Producer.h"
#include "pulsar/Schema.h"
#include "systems_structs.pb.h"
#include <google/protobuf/util/json_util.h>

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

    auto pOptions = CelteNet::ProducerOptions{
        .conf = conf,
        .topic = options.topic,
        .then = [this](pulsar::Producer producer,
                       const pulsar::Result &result) {
          if (result != pulsar::ResultOk and options.onConnectError) {
            options.onConnectError();
            return;
          }
          _producer = std::make_shared<pulsar::Producer>(producer);
          _ready = true;
          if (options.onReady)
            options.onReady(*this);
        }};

    net.CreateProducer(pOptions);
  }

  template <typename Req>
  void Write(const Req &req,
             std::function<void(pulsar::Result)> onDelivered = nullptr) {
    static_assert(std::is_base_of<google::protobuf::Message, Req>::value,
                  "Req must be a protobuf message.");
    std::string j;
    if (not google::protobuf::util::MessageToJsonString(req, &j).ok()) {
      std::cerr << "Error while serializing request." << std::endl;
      return;
    }
    auto message = pulsar::MessageBuilder().setContent(j);
    auto producerKeepAlive = _producer;
    auto pendingKeepAlive = _pending;
    (*pendingKeepAlive)++;
    producerKeepAlive->sendAsync(
        message.build(),
        [this, onDelivered, pendingKeepAlive, producerKeepAlive](
            pulsar::Result result, const pulsar::MessageId &messageId) {
          (*pendingKeepAlive)--;
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
  std::shared_ptr<pulsar::Producer> _producer;
  std::atomic_bool _ready;
  std::shared_ptr<std::atomic_int> _pending =
      std::make_shared<std::atomic_int>(0);
};
} // namespace net
} // namespace celte
