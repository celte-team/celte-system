#pragma once
#include "kafka/Properties.h"
#include "queue.hpp"
#include <atomic>
#include <boost/thread.hpp>
#include <functional>
#include <kafka/AdminClient.h>
#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>
#include <unordered_map>

namespace celte {
namespace nl {

/**
 * @brief KafkaPool manages sending and receiving messages from Kafka.
 * It handles creating consumers and producers.
 */
class KafkaPool {
  friend class kafka::clients::consumer::KafkaConsumer;
  friend class kafka::clients::producer::KafkaProducer;

public:
  struct Options {
    std::string bootstrapServers = "localhost:80";
  };

  /**
   * @brief MessageCallbacks are functions associated with a
   * particular topic. They will be called on messages polled from that topic
   * with the consumer record passed as argument.
   */
  using MessageCallback = std::function<void(
      const kafka::clients::consumer::ConsumerRecord &record)>;

  /**
   * @struct SubscribeOptions
   * @brief Options for subscribing to a Kafka topic.
   *
   * This structure holds various options that can be configured when
   * subscribing to a Kafka topic.
   *
   * @var std::string SubscribeOptions::topic
   * The name of the Kafka topic to subscribe to. Default is an empty string.
   *
   * @var bool SubscribeOptions::autoCreateTopic
   * Flag indicating whether the topic should be automatically created if it
   * does not exist. Default is true.
   *
   * @var std::string SubscribeOptions::groupId
   * The consumer group ID to use for the subscription. Default is an empty
   * string. Only one consumer can exist for a given group.
   *
   * @var std::map<std::string, std::string> SubscribeOptions::extraProps
   * Additional properties to configure the subscription. Default is an empty
   * map.
   *
   * @var bool SubscribeOptions::autoPoll
   * Flag indicating whether messages should be automatically polled. Default is
   * true. If set to false, the user must manually call the Poll method to
   * receive messages. The user should wait for a few ms before calling Poll as
   * the creation of a consumer takes some time to process in the kafka cluster.
   *
   * @var MessageCallback SubscribeOptions::callback
   * Callback function to handle messages. Default is nullptr.
   */
  struct SubscribeOptions {
    std::string topic = "";
    std::string groupId = "";
    bool autoCreateTopic = true;
    std::map<std::string, std::string> extraProps = {};
    bool autoPoll = true;
    MessageCallback callback = nullptr;
  };

  KafkaPool(const Options &options);
  ~KafkaPool();

  /**
   * @brief Subscribe to a topic with a callback function. The function will
   * only be exectuted if the CatchUp method is called.
   *
   * @param options
   */
  void Subscribe(const SubscribeOptions &options);

  /**
   * @brief Unsubscribes a group of consumers from the given topic.
   * @param topic The topic to unsubscribe from.
   * @param groupId The group ID to unsubscribe. Default is an empty string.
   * @param autoPoll Flag to indicate if the group should be looked up for in
   * the list of consumers that are automatically polled. Default is true.
   */
  void Unsubscribe(const std::string &topic, const std::string &groupId = "",
                   bool autoPoll = true);

  /**
   * @brief Asynchronously sends a message to kafka.
   */
  inline void Send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered) {
    __send(record, onDelivered);
  }

  struct SendOptions {
    std::string topic = "";
    std::map<std::string, std::string> headers = {};
    std::string value = "";
    std::function<void(const kafka::clients::producer::RecordMetadata &,
                       kafka::Error)>
        onDelivered = nullptr;
    bool autoCreateTopic = true;
  };

  /**
   * @brief Sends a message to Kafka.
   */
  void Send(const SendOptions &options);

  // void Send(const std::string &message)

  /**
   * @brief Executes all the callbacks associated with all the messages received
   * since the last time this method was called.
   *
   * This method is blocking!
   *
   * If there are too many tasks to complete, this method will not take up any
   * new task after maxBlockingMs milliseconds.
   *
   * @param maxBlockingMs
   */
  void CatchUp(unsigned int maxBlockingMs = 10);

  /**
   * @brief Manually polls messages from Kafka for the given group id, if the
   * group id has been registered for manual polling.
   *
   * @exceptions std::out_of_range if the group id has not been registered for
   * manual polling.
   *
   * @param groupId The group id to poll messages for.
   * @param pollTimeoutMs The maximum time to wait for messages in milliseconds.
   * Default is 100.
   *
   * @return True if messages were polled, false if an error occured.
   */
  bool Poll(const std::string &groupId, unsigned int pollTimeoutMs = 100);

private:
  /**
   * @brief Initializes the KafkaPool's threads, consumers and producers.
   */
  void __init();

  /**
   * @brief Consumer thread job. Polls messages from Kafka and stores them in
   * the _records queue.
   */
  void __consumerJob();

  /**
   * @brief Creates a topic in kafka if it does not exist yet.
   */
  void __createTopicIfNotExists(const std::string &topic, int numPartitions,
                                int replicationFactor);

  /**
   * @brief Emplaces a consumer in the _consumers or _manualConsumers map.
   * If the comsumer already exists for the given group id, nothing is done.
   */
  void __emplaceConsumerIfNotExists(const std::string &groupId,
                                    const kafka::Properties &props,
                                    bool autoPoll);

  /**
   * @brief Sends a message to Kafka, but updates the headers to include the
   * UUID of the runtime in the headers.
   */
  void __send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  Options _options;
  kafka::Properties _consumerProps;
  kafka::Properties _producerProps;

  // map of topic -> consumer scheduled for automatic polling
  std::unordered_map<std::string, kafka::clients::consumer::KafkaConsumer>
      _consumers;

  // map of topic -> consumer for manual polling
  std::unordered_map<std::string, kafka::clients::consumer::KafkaConsumer>
      _manualConsumers;

  kafka::clients::producer::KafkaProducer _producer;

  std::unordered_map<std::string, MessageCallback> _callbacks;
  std::shared_ptr<boost::mutex> _mutex;

  ubn::queue<kafka::clients::consumer::ConsumerRecord> _records;
  std::atomic_bool _running;

  boost::thread _consumerThread;
};
} // namespace nl
} // namespace celte