#include "CelteRuntime.hpp"
#include "KPool.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <iterator>
#include <memory>

namespace celte {
namespace nl {

KPool::KPool(const Options &options)
    : _options(options), _running(false), _records(100),
      _consumerMutex(new boost::mutex),
      _consumerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.auto.commit", {"true"}},
          {"log_level", {"3"}},
          {"retries", {"2"}},
          {"heartbeat.interval.ms", {"1000"}},
          {"session.timeout.ms", {"60000"}},
      }),
      _producerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.idempotence", {"true"}},
      }) {
  for (int i = 0; i < _options.numGeneralPurposeConsumerThreads; i++) {
    _consumerWorkers.emplace_back(std::make_shared<ConsumerWorker>(_records));
  }
}

KPool::~KPool() { _running = false; }

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

  // push the topics to the list of incoming subscriptions
  _subscriptionsToImplement.push(
      SubscriptionTask{.consumerGroupId = options.groupId,
                       .newSubscriptions = topics,
                       .useDedicatedThread = options.useDedicatedThread});
}

void KPool::RegisterTopicCallback(const std::string &topic,
                                  MessageCallback callback) {
  _callbacks[topic] = callback;
}

void KPool::Unsubscribe(const std::string &topic, const std::string &groupId,
                        bool autoPoll) {
  throw std::runtime_error("Not implemented");
}

void KPool::CatchUp(unsigned int maxBlockingMs) {
  auto startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  while (std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()) -
                 startMs <
             std::chrono::milliseconds(maxBlockingMs) &&
         !_records.empty()) {
    auto record = _records.pop();
    _callbacks[record.topic()](record);
  }
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

void KPool::Connect() {
  _producer.emplace(_producerProps);
  __initAdminClient();
  _running = true;
  // _consumerThread = boost::thread(&KPool::__consumerJob, this);
}

void KPool::CommitSubscriptions() {
  // convert the queue to a vector and clear it
  std::vector<SubscriptionTask> subscriptions;
  while (!_subscriptionsToImplement.empty()) {
    subscriptions.push_back(_subscriptionsToImplement.pop());
  }

  // Group subscriptions by dedicated thread / non-dedicated thread
  std::vector<SubscriptionTask> subscriptionsDedicated;
  std::vector<SubscriptionTask> subscriptionsNonDedicated;
  for (const auto &sub : subscriptions) {
    if (sub.useDedicatedThread) {
      subscriptionsDedicated.push_back(sub);
    } else {
      subscriptionsNonDedicated.push_back(sub);
    }
  }

  // Create consumers for the subscriptions that require a dedicated thread
  for (const auto &sub : subscriptionsDedicated) {
    auto props = _consumerProps;
    // props.put("group.id", sub.consumerGroupId);
    auto consumerWorker = std::make_shared<ConsumerWorker>(_records);
    auto consumer =
        std::make_shared<kafka::clients::consumer::KafkaConsumer>(props);
    consumer->subscribe(sub.newSubscriptions);
    consumerWorker->AddConsumer(consumer);
    _consumerWorkersDedicated.push_back(consumerWorker);
  }

  // regrouping all the topics (in the future, split by consumer group)
  kafka::Topics topics;
  for (const auto &sub : subscriptionsNonDedicated) {
    topics.insert(sub.newSubscriptions.begin(), sub.newSubscriptions.end());
  }

  // Get the worker with the less work
  auto minWorker = _consumerWorkers[0];
  for (auto &worker : _consumerWorkers) {
    if (worker->GetNumConsumers() < minWorker->GetNumConsumers()) {
      minWorker = worker;
    }
  }

  // Create consumers for the rest of the subscriptions
  auto consumer =
      std::make_shared<kafka::clients::consumer::KafkaConsumer>(_consumerProps);
  consumer->subscribe(topics);
  minWorker->AddConsumer(consumer);
}

void KPool::__initAdminClient() {
  kafka::Properties props;
  props.put("bootstrap.servers", _options.bootstrapServers);
  props.put("retries", "1");
  _adminClient.emplace(props);
}

bool KPool::__createTopicIfNotExists(std::set<std::string> &topics,
                                     int numPartitions, int replicationFactor) {
  if (!_adminClient) {
    __initAdminClient();
  }

  auto existingTopics =
      _adminClient.value().listTopics(std::chrono::milliseconds(1000));
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

  auto createResult = _adminClient.value().createTopics(
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

void KPool::Send(const SendOptions &options) {
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

void KPool::__send(
    kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDelivered) {
  {
    record.headers().push_back(
        kafka::Header{kafka::Header::Key{celte::tp::HEADER_PEER_UUID},
                      kafka::Header::Value{RUNTIME.GetUUID().c_str(),
                                           RUNTIME.GetUUID().size()}});

    if (!_producer.has_value()) {
      throw std::runtime_error("Producer not initialized");
    }
    _producer.value().send(record, onDelivered);
  }
}

void KPool::ResetConsumers() { _consumersAutoPoll.clear(); }

} // namespace nl
} // namespace celte
