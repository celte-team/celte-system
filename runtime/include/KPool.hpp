#pragma once
#include "KConsumerService.hpp"
#include "kafka/Properties.h"
#include "queue.hpp"
#include <atomic>
#include <boost/thread.hpp>
#include <functional>
#include <future>
#include <kafka/AdminClient.h>
#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace celte {
namespace nl {

/**
 * @class KPool
 * @brief Manages Kafka producer and consumer operations.
 */
class KPool {
public:
  /**
   * @struct Options
   * @brief Configuration options for KPool.
   */
  struct Options {
    std::string bootstrapServers = "localhost:80"; ///< Kafka bootstrap servers.
  };

  /**
   * @struct SubscriptionTask
   * @brief Represents a subscription task.
   */
  struct SubscriptionTask {
    kafka::Topics newSubscriptions = {}; ///< New subscriptions.
    std::function<void()> then =
        nullptr; ///< Callback to execute after subscription.
  };

  using MessageCallback = std::function<void(
      const kafka::clients::consumer::ConsumerRecord &record)>;

  /**
   * @struct SubscribeOptions
   * @brief Options for subscribing to topics.
   */
  struct SubscribeOptions {
    std::vector<std::string> topics =
        std::vector<std::string>(); ///< Topics to subscribe to.
    bool autoCreateTopic =
        true; ///< Automatically create topics if they do not exist.
    std::vector<MessageCallback> callbacks = {}; ///< Callbacks for each topic.
    std::function<void()> then =
        nullptr; ///< Callback to execute after subscription.
  };

  /**
   * @struct SendOptions
   * @brief Options for sending messages.
   */
  struct SendOptions {
    std::string topic = ""; ///< Topic to send the message to.
    std::map<std::string, std::string> headers =
        {};                 ///< Headers for the message.
    std::string value = ""; ///< Message value.
    std::function<void(const kafka::clients::producer::RecordMetadata &,
                       kafka::Error)>
        onDelivered = nullptr; ///< Callback to execute after message delivery.
    bool autoCreateTopic =
        false; ///< Automatically create topic if it does not exist.
  };

  /**
   * @brief Constructor for KPool.
   * @param options Configuration options.
   */
  KPool(const Options &options);

  /**
   * @brief Destructor for KPool.
   */
  ~KPool();

  /**
   * @brief Connects to Kafka.
   */
  void Connect();

  /**
   * @brief Sends a message with specified options.
   * @param options Options for sending the message.
   */
  void Send(const SendOptions &options);

  /**
   * @brief Sends a message.
   * @param record The message to send.
   * @param onDelivered Callback to execute after message delivery.
   */
  void Send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  /**
   * @brief Subscribes to topics with specified options. Nothing is done until
   * CommitSubscriptions is called.
   *
   * @warning The subscription is asynchronous ! use .then in the
   * options to provide a callback. Note the this callback is executed
   * synchronously when CatchUp is first called after the subscription is
   * completed.
   * @param options Options for subscribing to topics.
   */
  void Subscribe(const SubscribeOptions &options);

  /**
   * @brief Registers a callback for a specific topic.
   * @param topic The topic to register the callback for.
   * @param callback The callback to register.
   */
  void RegisterTopicCallback(const std::string &topic,
                             MessageCallback callback);

  /**
   * @brief Commits the current subscriptions.
   */
  void CommitSubscriptions();

  /**
   * @brief Processes pending tasks and messages.
   * @param maxBlockingMs Maximum time to block in milliseconds.
   */
  void CatchUp(unsigned int maxBlockingMs = 100);

  /**
   * @brief Creates a topic if it does not exist.
   * @param topic The topic to create.
   * @param numPartitions Number of partitions for the topic.
   * @param replicationFactor Replication factor for the topic.
   */
  void CreateTopicIfNotExists(std::string &topic, int numPartitions,
                              int replicationFactor);

  /**
   * @brief Creates multiple topics if they do not exist.
   * @param topics The topics to create.
   * @param numPartitions Number of partitions for the topics.
   * @param replicationFactor Replication factor for the topics.
   */
  void CreateTopicsIfNotExist(std::vector<std::string> &topics,
                              int numPartitions, int replicationFactor);

  /**
   * @brief Resets the Kafka consumers.
   */
  void ResetConsumers();

private:
  struct BatchSubscription {
    std::future<void> future;
    std::vector<std::function<void()>> thens;

    BatchSubscription(std::future<void> future,
                      std::vector<std::function<void()>> thens)
        : future(std::move(future)), thens(std::move(thens)) {}
  };

  void __send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  bool __createTopicIfNotExists(std::set<std::string> &topics,
                                int numPartitions, int replicationFactor);

  void __initAdminClient();

  Options _options;

  kafka::Properties _producerProps;
  kafka::Properties _consumerProps;
  std::unique_ptr<kafka::clients::producer::KafkaProducer> _producer;
  std::unique_ptr<kafka::clients::admin::AdminClient> _adminClient;
  ubn::queue<SubscriptionTask> _subscriptionsToImplement;
  ubn::queue<kafka::clients::consumer::ConsumerRecord> _records;
  std::unordered_map<std::string, MessageCallback> _callbacks;
  KConsumerService _consumerService;
};
} // namespace nl
} // namespace celte