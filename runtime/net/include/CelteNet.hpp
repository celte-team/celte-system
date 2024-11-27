#pragma once
#include "queue.hpp"
#include <functional>
#include <memory>
#include <pulsar/Client.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace celte {
namespace net {
class CelteNetException : public std::exception {
public:
  CelteNetException(const std::string &message) : message(message) {}

  const char *what() const noexcept override { return message.c_str(); }

private:
  std::string message;
};

class CelteNetTimeoutException : public CelteNetException {
public:
  CelteNetTimeoutException()
      : CelteNetException("Timeout while connecting to CELTE") {}
};

class CelteNet {
public:
  static CelteNet &Instance();

  struct ProducerOptions {
    pulsar::ProducerConfiguration conf; // The producer configuration
    std::string topic;                  // The topic to produce to
    std::function<void(pulsar::Producer, const pulsar::Result &)> then =
        nullptr; // called in the main thread once the producer is created.
    std::function<void(pulsar::Producer, const pulsar::Result &)> thenAsync =
        nullptr; // called once the producer is created.
  };

  // subscriptions are performed asynchronously
  struct SubscribeOptions {
    std::vector<std::string> topics;    // The topics to subscribe to
    std::string subscriptionName;       // The subscription name
    pulsar::ConsumerConfiguration conf; // The consumer configuration
    std::function<void(const pulsar::Consumer, const pulsar::Result &)> then =
        nullptr; // called once the subscription is done.
    std::function<void(const pulsar::Consumer &, const pulsar::Result &)>
        thenAsync = nullptr; // called once the subscription is done.
    std::function<void(pulsar::Consumer &, const pulsar::Message &)>
        messageHandler = nullptr; // callback called when a message is received
                                  // by the consumer.
  };

  /**
   * @brief Connects to the cluster.
   * @exception CelteNetTimeoutException if the connection times out.
   *
   * @param brokers The list of brokers to connect to.
   * @param timeoutMs The timeout in milliseconds.
   */
  void Connect(const std::string &brokers = "localhost:6650",
               int timeoutMs = 1000);

  inline pulsar::Client &GetClient() {
    if (!_client) {
      throw CelteNetException("Client not initialized");
    }
    return *_client;
  }

  inline void PushThen(std::function<void()> then) { _thens.push(then); }

  void CreateProducer(ProducerOptions &options);
  void CreateConsumer(SubscribeOptions &options);

  void ExecThens();

private:
  /**
   * @brief Initializes the configuration and the client.
   */
  void __init(const std::string &brokers = "pulsar://localhost:6650",
              int timeoutMs = 1000);

  std::unique_ptr<pulsar::Client> _client;

  ubn::queue<std::function<void()>> _thens;
};
} // namespace net
} // namespace celte