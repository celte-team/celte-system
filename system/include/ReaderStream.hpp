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
#include <condition_variable>
#include <google/protobuf/util/json_util.h>
#include <mutex>

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
    std::function<void(const pulsar::Consumer &, Req)> messageHandler = nullptr;
    std::function<void()> onReady = nullptr;
    std::function<void()> onConnectError = nullptr;
  };

  ReaderStream() {
    _clientRef = CelteNet::Instance().GetClientPtr();
    _consumer = std::make_shared<pulsar::Consumer>();
  }
  ~ReaderStream() {
    if (!*_closed)
      Close();
  }
  inline void Close() {
    if (*_closed) {
      return; // already closed
    }

    auto topic = _consumer->getTopic();
    *_closed = true;
    if (_pendingMessages->load() > 0) {
      BlockUntilNoPending();
    }

    auto consumerKeepAlive = _consumer; // keep the consumer alive until
                                        // the close is done
    _consumer->closeAsync([consumerKeepAlive](pulsar::Result res) {
      if (res != pulsar::ResultOk) {
        std::cerr << "Error closing consumer: " << res << std::endl;
      }
    });
  }

  /// @brief Blocks until the pending message counter has reached zero.
  void BlockUntilNoPending();

  bool Ready() { return _ready; }

  template <typename Req> void Open(Options<Req> &options) {
    static_assert(std::is_base_of<google::protobuf::Message, Req>::value,
                  "Req must be a protobuf message.");
    RUNTIME.ScheduleAsyncIOTask([this, options = std::move(options)] mutable {
      if (!__subscribe<Req>(options)) {
        std::cerr << "Failed to subscribe to topics." << std::endl;
        return;
      }
      std::function<void(const pulsar::Consumer &, Req)> handler =
          options.messageHandler;

      auto consumer = _consumer; // copy for memory safety
      _messageHandler = [this, options, handler = std::move(handler),
                         consumer](const std::string &data) mutable {
        Req req;
        if (google::protobuf::util::JsonStringToMessage(data, &req).ok()) {
          handler(*consumer, req);
        }
      };

      __startPolling(consumer);
      _ready = true; // mark the stream as ready
      if (options.onReady) {
        options.onReady();
      }
    }); // end of async task
  }

private:
  template <typename Req> bool __subscribe(Options<Req> &options) {
    auto &client = CelteNet::Instance().GetClient();
    pulsar::ConsumerConfiguration conf;
    if (options.exclusive)
      throw std::runtime_error("Exclusive consumer type is not supported in "
                               "ReaderStream anymore. It is deprecated");
    conf.setConsumerType(pulsar::ConsumerShared);

    std::string subscriptionName = options.subscriptionName;
    if (subscriptionName.empty()) {
      boost::uuids::uuid uuid = boost::uuids::random_generator()();
      subscriptionName = boost::uuids::to_string(uuid);
    }

    bool notSub = true;
    pulsar::Result res;
    int retry = 10;
    while (notSub && retry > 0) {
      try {
        res = client.subscribe(options.topics, subscriptionName, conf,
                               *_consumer);
        if (res == pulsar::ResultOk) {
          notSub = false; // successfully subscribed
          break;
        }
      } catch (...) {
        std::cerr << "Failed to subscribe to topics, retrying..." << std::endl;

        continue; // retry subscription
      }
      retry--;
    }

    if (res != pulsar::ResultOk) {
      if (options.onConnectError) {
        options.onConnectError();
      }
      std::cerr << "Error subscribing to topic: " << res << std::endl;
      return false;
    }
    return true;

    // std::mutex mtx;
    // std::condition_variable cv;
    // bool done = false;

    // // std::unique_lock<std::mutex> lock(mtx);

    // client.subscribeAsync(
    //     options.topics, subscriptionName, conf,
    //     [this, options = std::move(options), &mtx, &done,
    //      &cv](const pulsar::Result &res, pulsar::Consumer consumer) {
    //       std::lock_guard<std::mutex> guard(mtx);
    //       if (res != pulsar::ResultOk) {
    //         if (options.onConnectError) {
    //           options.onConnectError();
    //         }
    //         std::cerr << "Error subscribing to topic: " << res <<
    //         std::endl; done = true; cv.notify_all(); return;
    //       }
    //       _consumer =
    //       std::make_shared<pulsar::Consumer>(std::move(consumer)); if
    //       (options.onReady) {
    //         options.onReady();
    //       }
    //       done = true;
    //       cv.notify_all();
    //     });

    // cv.wait(lock, [&done]() { return done; });
    // _ready = true;
    // return true;
  }

  void __startPolling(std::shared_ptr<pulsar::Consumer> consumer);

protected:
  std::shared_ptr<pulsar::Client>
      _clientRef; ///< used for RAII, keeps the client alive until the stream
                  ///< is closed
  std::shared_ptr<pulsar::Consumer> _consumer;
  std::atomic_bool _ready = false;
  std::shared_ptr<std::atomic_bool> _closed =
      std::make_shared<std::atomic_bool>(false);
  std::shared_ptr<std::atomic_int> _pendingMessages =
      std::make_shared<std::atomic_int>(0);
  std::function<void(const std::string &)> _messageHandler;
};
} // namespace net
} // namespace celte
