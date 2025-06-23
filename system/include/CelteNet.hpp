#pragma once
#include <functional>
#include <memory>
#include <pulsar/Client.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace celte {
namespace net {
/**
 * @brief Exception thrown by the CelteNet class.
 */
class CelteNetException : public std::exception {
public:
  CelteNetException(const std::string &message) : message(message) {}

  const char *what() const noexcept override { return message.c_str(); }

private:
  std::string message;
};

/**
 * @brief Exception thrown when the connection to the cluster times out.
 */
class CelteNetTimeoutException : public CelteNetException {
public:
  CelteNetTimeoutException()
      : CelteNetException("Timeout while connecting to CELTE") {}
};

/**
 * @brief Singleton class that manages the connection to the cluster.
 * Used by the ReaderStream and WriterStream classes to create producers and
 * consumers.
 */
class CelteNet {
public:
  /**
   * @brief Returns the singleton instance of the class.
   */
  static CelteNet &Instance();

  /**
   * @brief Additional options for the producer, adding onto the existing
   * puslar producer configuration.
   *
   */
  struct ProducerOptions {
    pulsar::ProducerConfiguration conf; ///< Pulsar configuration.
    std::string topic;                  ///< The topic to produce to.
    std::function<void(pulsar::Producer, const pulsar::Result &)> then =
        nullptr; ///< called in the main thread once the producer is created.
    // std::function<void(pulsar::Producer, const pulsar::Result &)> thenAsync =
    //     nullptr; ///< called asynchronously once the producer is created.
  };

  /**
   * @brief Additional options for the consumer, adding onto the existing
   * pulsar consumer configuration.
   *
   */
  struct SubscribeOptions {
    std::vector<std::string> topics;    ///< List of topics to subscribe to.
    std::string subscriptionName;       ///< The name of the subscription.
    pulsar::ConsumerConfiguration conf; ///< Pulsar configuration.
    std::function<void(const pulsar::Consumer &, const pulsar::Result &)> then =
        nullptr; ///< called once the subscription is done, in the main thread.
    std::function<void(pulsar::Consumer &, const pulsar::Message &)>
        messageHandler = nullptr; ///< callback called when a message is
                                  ///< received by the consumer.
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

  /**
   * @brief Returns a reference to the pulsar client in case any
   * low-level operations are needed.
   */
  inline pulsar::Client &GetClient() {
    if (!_client) {
      throw CelteNetException("Client not initialized");
    }
    return *_client;
  }

  inline std::shared_ptr<pulsar::Client> GetClientPtr() {
    if (!_client) {
      throw CelteNetException("Client not initialized");
    }
    return _client;
  }

  void CreateProducer(ProducerOptions &options);
  void CreateConsumer(SubscribeOptions &options);

private:
  /**
   * @brief Initializes the configuration and the client.
   */
  void __init(const std::string &brokers = "pulsar://localhost:6650",
              int timeoutMs = 10000);

  std::shared_ptr<pulsar::Client> _client;
};
} // namespace net
} // namespace celte