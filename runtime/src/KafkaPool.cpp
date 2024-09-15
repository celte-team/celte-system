#include "KafkaPool.hpp"
#include <chrono>

namespace celte {
namespace nl {

KafkaPool::KafkaPool(const Options &options)
    : _options(options), _running(false), _records(100),
      _mutex(new boost::mutex),
      _consumerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.auto.commit", {"true"}},
      }),
      _producerProps(kafka::Properties({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.idempotence", {"true"}},
      })),
      _producer(_producerProps) {

  __init();
}

KafkaPool::~KafkaPool() {
  _running = false;
  _consumerThread.join();
}

void KafkaPool::__init() {
  _running = true;
  _consumerThread = boost::thread(&KafkaPool::__consumerJob, this);
}

void KafkaPool::__consumerJob() {
  while (_running) {
    for (auto &consumer : _consumers) {

      auto records = consumer.second.poll(std::chrono::milliseconds(100));
      for (auto &record : records) {
        std::cout << "auto poll record" << std::endl;
        _records.push(record);
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
  {
    boost::lock_guard<boost::mutex> lock(*_mutex);
    _callbacks[ops.topic] = ops.callback;
  }

  if (ops.autoCreateTopic) {
    __createTopicIfNotExists(ops.topic, 1, 1);
  }

  auto props = _consumerProps;
  if (!ops.groupId.empty()) {
    props.put("group.id", ops.groupId);
    for (auto &prop : ops.extraProps) {
      props.put(prop.first, prop.second);
    }
  }

  __emplaceConsumerIfNotExists(ops.groupId, props, ops.autoPoll);

  auto &consumer = (ops.autoPoll) ? _consumers.at(ops.groupId)
                                  : _manualConsumers.at(ops.groupId);

  auto subscriptions = consumer.subscription();
  subscriptions.insert(ops.topic);
  consumer.unsubscribe(); // TODO: check if this is necessary
  consumer.subscribe(subscriptions);
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

void KafkaPool::__createTopicIfNotExists(const std::string &topic,
                                         int numPartitions,
                                         int replicationFactor) {
  kafka::clients::admin::AdminClient adminClient(_consumerProps);

  auto topics = adminClient.listTopics();
  for (auto &topicName : topics.topics) {
    if (topicName == topic)
      return;
  }

  auto createResult =
      adminClient.createTopics({topic}, numPartitions, replicationFactor);
  if (!createResult.error ||
      createResult.error.value() == RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS) {
    return;
  }
}

bool KafkaPool::Poll(const std::string &groupId, unsigned int pollTimeoutMs) {
  try {
    auto &consumer = _manualConsumers.at(groupId);
    auto records = consumer.poll(std::chrono::milliseconds(pollTimeoutMs));
    for (auto &record : records) {
      std::cout << "manual poll record" << std::endl;
      _records.push(record);
    }
  } catch (const std::out_of_range &e) {
    std::cout << "Group ID " << groupId << " not found" << std::endl;
    return false;
  }
  return true;
}

} // namespace nl
} // namespace celte