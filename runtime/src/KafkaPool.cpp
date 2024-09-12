#include "KafkaPool.hpp"
#include <chrono>

namespace celte {
namespace nl {

KafkaPool::KafkaPool(const Options &options)
    : _options(options), _running(false), _records(100),
      _consumerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.auto.commit", {"true"}},
      }),
      _producerProps(kafka::Properties({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.idempotence", {"true"}},
      })),
      _consumer(_consumerProps), _producer(_producerProps) {
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
    auto records = _consumer.poll(std::chrono::milliseconds(100));
    for (auto &record : records) {
      _records.push(record);
    }
  }

  _consumer.unsubscribe();
}

void KafkaPool::Subscribe(const std::string &topic, MessageCallback callback,
                          bool autoCreateTopic) {
  _callbacks[topic] = callback;

  if (autoCreateTopic) {
    __createTopicIfNotExists(topic, 1, 1);
  }
  _consumer.subscribe({topic});
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

} // namespace nl
} // namespace celte