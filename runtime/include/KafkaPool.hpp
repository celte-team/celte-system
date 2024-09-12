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
public:
  struct Options {
    std::string bootstrapServers;
  };

  /**
   * @brief MessageCallbacks are functions associated with a
   * particular topic. They will be called on messages polled from that topic
   * with the consumer record passed as argument.
   */
  using MessageCallback = std::function<void(
      const kafka::clients::consumer::ConsumerRecord &record)>;

  //   struct SendData {
  //     kafka::clients::producer::ProducerRecord record;
  //     const std::function<void(const kafka::clients::producer::RecordMetadata
  //     &,
  //                              kafka::Error)>
  //         onDeliveryError;
  //   };

  KafkaPool(const Options &options);
  ~KafkaPool();

  /**
   * @brief Subscribe to a topic with a callback function. The function will
   * only be exectuted if the CatchUp method is called.
   */
  void Subscribe(const std::string &topic, MessageCallback callback,
                 bool autoCreateTopic = true);

  /**
   * @brief Asynchronously sends a message to kafka.
   */
  inline void Send(
      const kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDeliveryError) {
    _producer.send(record, onDeliveryError);
  }

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

  Options _options;
  kafka::Properties _consumerProps;
  kafka::Properties _producerProps;
  kafka::clients::consumer::KafkaConsumer _consumer;
  kafka::clients::producer::KafkaProducer _producer;

  std::unordered_map<std::string, MessageCallback> _callbacks;

  ubn::queue<kafka::clients::consumer::ConsumerRecord> _records;
  std::atomic_bool _running;

  boost::thread _consumerThread;
};
} // namespace nl
} // namespace celte