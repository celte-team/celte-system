#include "CelteNet.hpp"
#include "Runtime.hpp"
#include "pulsar/ConsoleLoggerFactory.h"

using namespace celte::net;

CelteNet &CelteNet::Instance() {
  static CelteNet instance;
  return instance;
}

void CelteNet::Connect(const std::string &brokers, int timeoutMs) {
  __init(brokers, timeoutMs);
}

void CelteNet::__init(const std::string &brokers, int timeoutMs) {
  pulsar::ClientConfiguration conf;
  conf.setOperationTimeoutSeconds(timeoutMs / 1000);
  conf.setIOThreads(1);
  conf.setMessageListenerThreads(1);
  conf.setUseTls(false);
  conf.setLogger(new pulsar::ConsoleLoggerFactory(pulsar::Logger::LEVEL_WARN));

  std::string pulsarBrokers = "pulsar://" + brokers;
  _client = std::make_unique<pulsar::Client>(pulsarBrokers, conf);
}

void CelteNet::CreateProducer(ProducerOptions &options) {
  if (!_client) {
    throw CelteNetException("Client not initialized");
  }

  _client->createProducerAsync(
      options.topic, options.conf,
      [this, options](pulsar::Result result, pulsar::Producer newProducer) {
        if (options.then) {
          RUNTIME.ScheduleSyncTask([options, newProducer, result]() {
            options.then(newProducer, result);
          });
        }
        if (options.thenAsync)
          options.thenAsync(newProducer, result);
      });
}

/**
 * @brief Creates a Pulsar consumer asynchronously.
 *
 * Configures the subscription using the provided options, sets a message listener that copies incoming messages to
 * ensure controlled message lifecycle management, calls the specified message handler, and acknowledges the original message.
 * The resulting consumer is delivered via asynchronous callbacks defined in the options.
 *
 * @param options Contains subscription configuration details including topics, subscription name, message handler,
 *                and callbacks for handling the consumer creation result.
 *
 * @throws CelteNetException if the client is not initialized or if the message handler is not set.
 */
void CelteNet::CreateConsumer(SubscribeOptions &options) {
  if (!_client) {
    throw CelteNetException("Client not initialized");
  }

  if (options.messageHandler == nullptr) {
    throw CelteNetException("Message handler not set");
  }

  options.conf.setMessageListener(
      [options](pulsar::Consumer &consumer, const pulsar::Message &msg) {
        auto msg_cpy =
            msg; // copy the message to avoid it being deleted without control
        options.messageHandler(consumer, msg_cpy);
        consumer.acknowledge(msg);
      });

  _client->subscribeAsync(
      options.topics, options.subscriptionName + "." + RUNTIME.GetUUID(),
      options.conf,
      [this, options](pulsar::Result result, pulsar::Consumer newConsumer) {
        if (options.thenAsync)
          options.thenAsync(newConsumer, result);
        if (options.then) {
          RUNTIME.ScheduleSyncTask([options, newConsumer, result]() {
            options.then(newConsumer, result);
          });
        }
      });
}
