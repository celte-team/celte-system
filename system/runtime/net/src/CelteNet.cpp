#include "CelteNet.hpp"

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
  conf.setOperationTimeoutSeconds(1);
  conf.setIOThreads(1);
  conf.setMessageListenerThreads(1);
  conf.setUseTls(false);

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
          _thens.push([options, newProducer, result]() {
            options.then(newProducer, result);
          });
          if (options.thenAsync)
            options.thenAsync(newProducer, result);
        }
      });
}

void CelteNet::CreateConsumer(SubscribeOptions &options) {
  if (!_client) {
    throw CelteNetException("Client not initialized");
  }

  if (options.messageHandler == nullptr) {
    throw CelteNetException("Message handler not set");
  }

  options.conf.setMessageListener(
      [options](pulsar::Consumer &consumer, const pulsar::Message &msg) {
        if (msg.getTopicName() != "persistent://public/default/global.clock") {
          std::cout << "Received message: " << msg.getDataAsString()
                    << std::endl;
        }
        options.messageHandler(consumer, msg);
      });

  _client->subscribeAsync(
      options.topics, options.subscriptionName, options.conf,
      [this, options](pulsar::Result result, pulsar::Consumer newConsumer) {
        if (options.thenAsync)
          options.thenAsync(newConsumer, result);
        if (options.then) {
          _thens.push([options, newConsumer, result]() {
            options.then(newConsumer, result);
          });
        }
      });
}

void CelteNet::ExecThens() {
  if (!_client) {
    return;
  }

  while (!_thens.empty()) {
    auto then = _thens.pop();
    then();
  }
}