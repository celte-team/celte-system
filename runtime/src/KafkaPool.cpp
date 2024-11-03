#include "CelteRuntime.hpp"
#include "KafkaPool.hpp"
#include "Logger.hpp"
#include "topics.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <set>

namespace celte {
namespace nl {

KafkaPool::KafkaPool(const Options &options)
    : _options(options), _running(false), _records(100),
      _mutex(new boost::mutex),
      _consumerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.auto.commit", {"true"}},
          {"log_level", {"3"}},
          {"retries", {"2"}},
      }), // Socket timeout

      _producerProps(kafka::Properties({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.idempotence", {"true"}},
      })),

      _producer(std::nullopt) {

  _producerProps.put("error_cb", [this](const kafka::Error &error) {
    logs::Logger::getInstance().err()
        << "Producer error: " << error.toString() << std::endl;
  });

  __init();
}

void KafkaPool::Connect() { _producer.emplace(_producerProps); }

kafka::clients::consumer::KafkaConsumer &
KafkaPool::GetConsumer(const std::string &topic) {
  return _consumers.at(topic);
}
KafkaPool::~KafkaPool() {
  _running = false;
  _consumerThread.join();
}

void KafkaPool::__init() {
  _running = true;
  _consumerThread = boost::thread(&KafkaPool::__consumerJob, this);
}

void KafkaPool::Send(const KafkaPool::SendOptions &options) {
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

  auto deliveryCb =
      [opts](const kafka::clients::producer::RecordMetadata &metadata,
             const kafka::Error &error) {
        if (opts->onDelivered) {
          opts->onDelivered(metadata, error);
        }
      };

  std::vector<kafka::Header> headers;
  for (auto &header : opts->headers) {
    headers.push_back(kafka::Header{
        kafka::Header::Key{header.first},
        kafka::Header::Value{header.second.c_str(), header.second.size()}});
  }
  record.headers() = headers;

  __send(record, deliveryCb);
}

void KafkaPool::__send(
    kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDelivered) {
  record.headers().push_back(
      kafka::Header{kafka::Header::Key{celte::tp::HEADER_PEER_UUID},
                    kafka::Header::Value{RUNTIME.GetUUID().c_str(),
                                         RUNTIME.GetUUID().size()}});
  if (!_producer.has_value()) {
    return;
  }
  _producer.value().send(record, onDelivered);
}

void KafkaPool::__consumerJob() {
  while (_running) {
    // avoid busy waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (auto &consumer : _consumers) {
      {
        boost::lock_guard<boost::mutex> lock(*_mutex);
        auto records = consumer.second.poll(std::chrono::milliseconds(100));
        for (auto &record : records) {
          _records.push(record);
        }
      }
    }
  }

  for (auto &consumer : _consumers) {
    consumer.second.unsubscribe();
  }
  {
    boost::lock_guard<boost::mutex> lock(*_mutex);
    _callbacks.clear();
  }
}

void KafkaPool::__emplaceConsumerIfNotExists(const std::string &groupId,
                                             const kafka::Properties &props,
                                             bool autoPoll) {
  if (autoPoll and _consumers.find(groupId) == _consumers.end()) {
    _consumers.emplace(std::piecewise_construct, std::forward_as_tuple(groupId),
                       std::forward_as_tuple(props));
  } else if (_manualConsumers.find(groupId) == _manualConsumers.end()) {
    _manualConsumers.emplace(std::piecewise_construct,
                             std::forward_as_tuple(groupId),
                             std::forward_as_tuple(props));
  }
}

void KafkaPool::Subscribe(const SubscribeOptions &ops) {
  if (ops.callbacks.size() != 0 and ops.topics.size() != ops.callbacks.size()) {
    logs::Logger::getInstance().err()
        << "Number of topics and callbacks must be the same" << std::endl;
    return;
  }

  // if callbacks is empty, we expect them to be set manually elsewhere. Else,
  // we do it now:
  if (ops.callbacks.size() == ops.topics.size()) {
    // registering the callbacks for each topic
    boost::lock_guard<boost::mutex> lock(*_mutex);
    for (int i = 0; i < ops.topics.size(); i++) {
      _callbacks[ops.topics[i]] = ops.callbacks[i];
    }
  }

  // auto create topics if needed
  auto topicsAsSet =
      std::set<std::string>(ops.topics.begin(), ops.topics.end());
  if (ops.autoCreateTopic and not __createTopicIfNotExists(topicsAsSet, 1, 1)) {
    logs::Logger::getInstance().err() << "Failed to create topics" << std::endl;
    throw std::runtime_error("Failed to create topic");
  }

  // assigning the group id
  auto props = _consumerProps;
  if (!ops.groupId.empty()) {
    props.put("group.id", ops.groupId);
    for (auto &prop : ops.extraProps) {
      props.put(prop.first, prop.second);
    }
  }

  // create a consumer for the group if it does not exist
  __emplaceConsumerIfNotExists(ops.groupId, props, ops.autoPoll);
  auto &consumer = (ops.autoPoll) ? _consumers.at(ops.groupId)
                                  : _manualConsumers.at(ops.groupId);

  // Get the current subscription list and update it with the new topics
  auto subscriptions = consumer.subscription();
  for (auto &topic : ops.topics) {
    subscriptions.insert(topic);
  }

  // Subscribe with the updated subscription list
  try {
    boost::lock_guard<boost::mutex> lock(*_mutex);
    consumer.subscribe(subscriptions);
  } catch (std::exception &e) {
    logs::Logger::getInstance().err() << e.what() << std::endl;
  }
}

void KafkaPool::Unsubscribe(const std::string &topic,
                            const std::string &groupId, bool autoPoll) {
  {
    boost::lock_guard<boost::mutex> lock(*_mutex);
    _callbacks.erase(topic);
  }

  // if the group does not exist, nothing to do
  try {
    auto &consumer =
        (autoPoll) ? _consumers.at(groupId) : _manualConsumers.at(groupId);
    std::set<std::string> subscriptions = consumer.subscription();
    subscriptions.erase(topic);
    consumer.unsubscribe(); // TODO: check if this is necessary
    consumer.subscribe(subscriptions);
  } catch (const std::out_of_range &e) {
    return;
  }
}

void KafkaPool::ResetConsumers() {
  _producer.value().close();
  _producer = std::nullopt;
  for (auto &consumer : _consumers) {
    consumer.second.unsubscribe();
    consumer.second.close();
  }
  {
    // boost::lock_guard<boost::mutex> lock(*_mutex);
    _callbacks.clear();
  }
  _consumers.clear();
}

void KafkaPool::CatchUp(unsigned int maxBlockingMs) {
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

bool KafkaPool::__createTopicIfNotExists(std::set<std::string> &topics,
                                         int numPartitions,
                                         int replicationFactor) {
  kafka::Properties props;
  props.put("bootstrap.servers", _options.bootstrapServers);
  props.put("retries", "1");
  kafka::clients::admin::AdminClient adminClient(props);
  auto existingTopics = adminClient.listTopics(std::chrono::milliseconds(1000));
  if (existingTopics.error) {
    logs::Logger::getInstance().err()
        << "Error listing topics: " << existingTopics.error.value()
        << std::endl;
    return false;
  }

  for (const auto &topic : existingTopics.topics) {
    topics.erase(topic);
  }

  if (topics.size() == 0) {
    return true;
  }

  auto createResult = adminClient.createTopics(
      topics, numPartitions, replicationFactor, kafka::Properties(),
      std::chrono::milliseconds(1000));
  if (!createResult.error ||
      createResult.error.value() == RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS) {
    return true;
  }
  return false;
}

bool KafkaPool::Poll(const std::string &groupId, unsigned int pollTimeoutMs) {
  try {
    auto &consumer = _manualConsumers.at(groupId);
    auto records = consumer.poll(std::chrono::milliseconds(pollTimeoutMs));
    for (auto &record : records) {
      _records.push(record);
    }
  } catch (const std::out_of_range &e) {
    logs::Logger::getInstance().err()
        << "Group ID " << groupId << " not found" << std::endl;
    return false;
  }
  return true;
}

} // namespace nl
} // namespace celte
