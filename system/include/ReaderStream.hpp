#pragma once
#include "CelteNet.hpp"
#include "Runtime.hpp"
#include "nlohmann/json.hpp"
#include "pulsar/Consumer.h"
#include "pulsar/ConsumerConfiguration.h"
#include "systems_structs.pb.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <google/protobuf/util/json_util.h>

namespace celte {
namespace net {

///@brief RAII class that decrements a counter when it goes out of scope, and
/// increments it when constructed.
struct PendingRefCount {
  PendingRefCount(std::atomic_int &counter);
  ~PendingRefCount();

private:
  std::atomic_int &_counter;
};

struct ReaderStream {
  template <typename Req> struct Options {
    std::string thisPeerUuid;
    std::vector<std::string> topics;
    std::string subscriptionName;
    bool exclusive = false;
    std::function<void(const pulsar::Consumer, Req)> messageHandlerSync =
        nullptr;
    std::function<void(const pulsar::Consumer, Req)> messageHandler = nullptr;
    std::function<void()> onReadySync = nullptr;
    std::function<void()> onConnectErrorSync = nullptr;
    std::function<void()> onReady = nullptr;
    std::function<void()> onConnectError = nullptr;
  };

  ReaderStream() { _clientRef = CelteNet::Instance().GetClientPtr(); }
  ~ReaderStream() { _consumer.close(); }

  inline void Close() {
    _closed = true;
    _consumer.close();
  }

  template <typename Req> void Open(Options<Req> &options) {
    static_assert(std::is_base_of<google::protobuf::Message, Req>::value,
                  "Req must be a protobuf message.");
    auto &net = CelteNet::Instance();
    auto conf = pulsar::ConsumerConfiguration();

    // is access mode exclusive? -> only this consumer can read from the topic
    if (options.exclusive)
      conf.setConsumerType(pulsar::ConsumerExclusive);
    else {
      conf.setConsumerType(pulsar::ConsumerShared);
    }

    // if subscription name is empty, set it to a random uuid
    if (options.subscriptionName.empty()) {
      boost::uuids::uuid uuid = boost::uuids::random_generator()();
      options.subscriptionName = boost::uuids::to_string(uuid);
    }

    CelteNet::SubscribeOptions subOps{
        .topics = options.topics,
        .subscriptionName = options.subscriptionName,
        .conf = conf,

        .then = // executed in the main thread after the consumer is created
        [this, options](pulsar::Consumer consumer,
                        const pulsar::Result &result) {
          if (result != pulsar::ResultOk and options.onConnectErrorSync) {
            options.onConnectErrorSync();
          }
          if (options.onReadySync)
            options.onReadySync();
        },

        .thenAsync = // executed after the consumer is created
        [this, options](pulsar::Consumer consumer,
                        const pulsar::Result &result) {
          if (result != pulsar::ResultOk and options.onConnectError) {
            options.onConnectError();
          }
          _consumer = consumer;
          _ready = true;
          if (options.onReady)
            options.onReady();
        },

        .messageHandler = // executed when a message is received
        [this, options](pulsar::Consumer consumer, const pulsar::Message &msg) {
          PendingRefCount prc(
              _pendingMessages); // RAII counter for pending handler messages.
          if (_closed) {
            return;
          }
          // if consumer is closed, don't handle the message
          if (not consumer.isConnected()) {
            return;
          }
          Req req;
          std::string data(static_cast<const char *>(msg.getData()),
                           msg.getLength());

          // { // don't remove this if its commented, someone will use it
          //   // debug
          //   if (msg.getTopicName().find("global.clock") == std::string::npos)
          //     std::cout << "[[ReaderStream]] handling message " << data
          //               << " from topic " << msg.getTopicName() << std::endl;
          // }

          if (!google::protobuf::util::JsonStringToMessage(data, &req).ok()) {
            std::cerr << "Error parsing message: " << data << std::endl;
            return;
          }
          if (options.messageHandler)
            options.messageHandler(consumer, req);
          if (options.messageHandlerSync)
            RUNTIME.ScheduleSyncTask([this, consumer, req, options]() {
              options.messageHandlerSync(consumer, req);
            });
          consumer.acknowledge(msg);
        }};
    net.CreateConsumer(subOps);
  }

  bool Ready() { return _ready; }

  /// @brief Blocks until the pending message counter has reached zero.
  void BlockUntilNoPending();

protected:
  std::shared_ptr<pulsar::Client>
      _clientRef; ///< used for RAII, keeps the client alive until the stream is
                  ///< closed
  pulsar::Consumer _consumer;
  std::atomic_bool _ready = false;
  std::atomic_bool _closed = false;
  std::atomic_int _pendingMessages = 0;
};
} // namespace net
} // namespace celte
