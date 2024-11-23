#include "CelteRuntime.hpp"
#include "KPool.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <iterator>
#include <memory>

namespace celte {
namespace nl {

KPool::KPool(const Options &options)
    : _options(options),
      // _running(false),
      _records(100), _consumerProps({
                         {"bootstrap.servers", {options.bootstrapServers}},
                         {"enable.auto.commit", {"true"}},
                         {"log_level", {"3"}},
                         {"retries", {"2"}},
                         {"heartbeat.interval.ms", {"1000"}},
                         {"session.timeout.ms", {"60000"}},
                         // {"group.id", {RUNTIME.GetUUID()}}
                     }),
      _producerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.idempotence", {"true"}},
      }),
      _consumerService(_consumerProps, 5, _records) {}

KPool::~KPool() {}

void KPool::Connect() {
  _producer =
      std::make_unique<kafka::clients::producer::KafkaProducer>(_producerProps);
  __initAdminClient();
  _consumerService.Start();
}

void KPool::CatchUp(unsigned int maxBlockingMs) {
  auto startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  // execute tasks from the network
  while (std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()) -
                 startMs <
             std::chrono::milliseconds(maxBlockingMs) &&
         !_records.empty()) {
    auto record = _records.pop();
    if (_callbacks.find(record.topic()) != _callbacks.end()) {
      _callbacks[record.topic()](record);
    }
  }
  _consumerService.ExecThens();
}

void KPool::__send(
    kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDelivered) {
  {
    record.headers().push_back(
        kafka::Header{kafka::Header::Key{celte::tp::HEADER_PEER_UUID},
                      kafka::Header::Value{RUNTIME.GetUUID().c_str(),
                                           RUNTIME.GetUUID().size()}});

    if (!_producer) {
      throw std::runtime_error("Producer not initialized");
    }
    _producer->send(record, onDelivered);
  }
}

void KPool::__initAdminClient() {
  kafka::Properties props;
  props.put("bootstrap.servers", _options.bootstrapServers);
  props.put("retries", "1");
  props.put("acks", "all");
  _adminClient = std::make_unique<kafka::clients::admin::AdminClient>(props);
}

bool KPool::__createTopicIfNotExists(std::set<std::string> &topics,
                                     int numPartitions, int replicationFactor) {
  if (!_adminClient) {
    __initAdminClient();
  }

  auto existingTopics =
      _adminClient->listTopics(std::chrono::milliseconds(1000));
  if (existingTopics.error) {
    logs::Logger::getInstance().err()
        << "Error listing topics: " << existingTopics.error.value()
        << std::endl;
    return false;
  }

  kafka::Topics topicsToCreate;
  std::set_difference(topics.begin(), topics.end(),
                      existingTopics.topics.begin(),
                      existingTopics.topics.end(),
                      std::inserter(topicsToCreate, topicsToCreate.begin()));

  if (topicsToCreate.size() == 0) {
    return true;
  }

  auto createResult = _adminClient->createTopics(
      topicsToCreate, numPartitions, replicationFactor, kafka::Properties(),
      std::chrono::milliseconds(1000));
  if (!createResult.error ||
      createResult.error.value() == RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS) {
    return true;
  }
  return false;
}

void KPool::Send(
    kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDelivered) {
  __send(record, onDelivered);
}

void KPool::Send(const KPool::SendOptions &options) {
  std::set<std::string> topics{options.topic};
  if (options.autoCreateTopic and not __createTopicIfNotExists(topics, 1, 1)) {
    logs::Logger::getInstance().err()
        << "Failed to create topic " << options.topic << std::endl;
    throw std::runtime_error("Failed to create topic");
  }

  auto opts = std::make_shared<SendOptions>(options);
  auto record = kafka::clients::producer::ProducerRecord(
      opts->topic, kafka::NullKey,
      kafka::Value(opts->value.c_str(), opts->value.size()));

  for (auto &header : opts->headers) {
    record.headers().push_back(kafka::Header{
        kafka::Header::Key{header.first},
        kafka::Header::Value{header.second.c_str(), header.second.size()}});
  }

  auto deliveryCb =
      [opts](const kafka::clients::producer::RecordMetadata &metadata,
             const kafka::Error &error) {
        if (opts->onDelivered) {
          opts->onDelivered(metadata, error);
        }
      };

  __send(record, deliveryCb);
}

void KPool::RegisterTopicCallback(const std::string &topic,
                                  MessageCallback callback) {
  _callbacks[topic] = callback;
}

void KPool::CreateTopicIfNotExists(std::string &topic, int numPartitions,
                                   int replicationFactor) {
  std::set<std::string> topics{topic};
  if (!__createTopicIfNotExists(topics, numPartitions, replicationFactor)) {
    logs::Logger::getInstance().err() << "Failed to create topic" << std::endl;
    throw std::runtime_error("Failed to create topic");
  }
}

void KPool::CreateTopicsIfNotExist(std::vector<std::string> &topics,
                                   int numPartitions, int replicationFactor) {
  std::set<std::string> topicsSet(topics.begin(), topics.end());
  if (!__createTopicIfNotExists(topicsSet, numPartitions, replicationFactor)) {
    logs::Logger::getInstance().err() << "Failed to create topics" << std::endl;
    throw std::runtime_error("Failed to create topic");
  }
}

void KPool::Subscribe(const SubscribeOptions &options) {
  kafka::Topics topics(options.topics.begin(), options.topics.end());

  // create the topics if needed
  if (options.autoCreateTopic and not __createTopicIfNotExists(topics, 1, 1)) {
    logs::Logger::getInstance().err() << "Failed to create topics" << std::endl;
    throw std::runtime_error("Failed to create topic");
  }

  //  the callbacks for each topic, if provided
  if (options.callbacks.size() != 0 and
      options.callbacks.size() == options.topics.size()) {
    for (int i = 0; i < options.topics.size(); i++) {
      _callbacks[options.topics[i]] = options.callbacks[i];
    }
  }

  {
    // debug
    std::cout << "subscribing to topics" << std::endl;
    for (const auto &topic : options.topics) {
      std::cout << "  - " << topic << std::endl;
    }
    std::cout << std::endl;
  }

  // push the topics to the list of incoming subscriptions
  _subscriptionsToImplement.push(SubscriptionTask{
      .newSubscriptions = topics,
      .then = options.then,
  });
}

void KPool::CommitSubscriptions() {
  std::set<std::string> topics;
  std::vector<std::function<void()>> thens;

  while (!_subscriptionsToImplement.empty()) {
    auto sub = _subscriptionsToImplement.pop();
    topics.insert(sub.newSubscriptions.begin(), sub.newSubscriptions.end());
    if (sub.then != nullptr) {
      thens.push_back(sub.then);
    }
  }

  _consumerService.ExecSubscriptions(topics, thens);
}

void KPool::ResetConsumers() {
  _consumerService.Stop();
  _consumerService.Start();
}

} // namespace nl
} // namespace celte