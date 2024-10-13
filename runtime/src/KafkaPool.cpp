#include "CelteRuntime.hpp"
#include "KafkaPool.hpp"
#include "Logger.hpp"
#include "topics.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>

namespace celte {
namespace nl {

KafkaPool::KafkaPool(const Options &options)
    : _options(options), _running(false), _records(100),
      _mutex(new boost::mutex),
      _consumerProps({
          {"bootstrap.servers", {options.bootstrapServers}},
          {"enable.auto.commit", {"true"}},
          {"session.timeout.ms", {"10000"}},
          // {""}
      }),
      // {"debug", {"all"}}}),
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

void KafkaPool::Send(const KafkaPool::SendOptions &options) {
  if (options.autoCreateTopic) {
    __createTopicIfNotExists(options.topic, 1, 1);
  }

  // wrapping the options in a shared ptr to avoid copying or dangling
  // references
  auto opts = std::make_shared<SendOptions>(options);
  auto record = kafka::clients::producer::ProducerRecord(
      opts->topic, kafka::NullKey,
      kafka::Value(opts->value.c_str(), opts->value.size()));

  // wrapping the error callback to capture the shared ptr and keep it
  // alive
  auto deliveryCb =
      [opts](const kafka::clients::producer::RecordMetadata &metadata,
             const kafka::Error &error) {
        if (opts->onDelivered) {
          opts->onDelivered(metadata, error);
        }
      };

  // Set the headers of the record to hold the name of the remote
  // procedure
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
  _producer.send(record, onDelivered);
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
  std::string uuid =
      boost::uuids::to_string(boost::uuids::random_generator()());
  props.put("client.id", uuid);

  __emplaceConsumerIfNotExists(ops.groupId, props, ops.autoPoll);

  auto &consumer = (ops.autoPoll) ? _consumers.at(ops.groupId)
                                  : _manualConsumers.at(ops.groupId);

  // Get the current subscription list
  auto subscriptions = consumer.subscription();

  // Add the new topic to the subscription list
  subscriptions.insert(ops.topic);

  try {
    boost::lock_guard<boost::mutex> lock(*_mutex);
    // Subscribe with the updated subscription list
    consumer.subscribe(subscriptions);
  } catch (kafka::KafkaException &e) {
    logs::Logger::getInstance().err()
        << "Error subscribing to topic " << ops.topic << std::endl;
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
  std::cout << "create topic " << topic << std::endl;
  auto topics = adminClient.listTopics();
  for (auto &topicName : topics.topics) {
    if (topicName == topic)
      return;
  }
  std::cout << "topic created successfully" << std::endl;
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