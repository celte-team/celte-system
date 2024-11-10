#pragma once
#include "kafka/Properties.h"
#include "queue.hpp"
#include <atomic>
#include <boost/thread.hpp>
#include <functional>
#include <kafka/AdminClient.h>
#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace celte {
namespace nl {
/**
 * @brief This class manages sending, receiving and processing messages from
 * Kafka. It handles creating consumers and producers.
 */
class KPool {
  friend class kafka::clients::consumer::KafkaConsumer;
  friend class kafka::clients::producer::KafkaProducer;

  // clang-format off
/* -------------------------------------------------------------------------- */
/*                          HELPER STRUCTS AND ENUMS                          */
/* -------------------------------------------------------------------------- */
  // clang-format on

public:
  /**
   * @brief This structure is used when initializing the KPool, to pass
   */
  struct Options {
    std::string bootstrapServers = "localhost:80";
  };

  struct SubscriptionTask {
    std::string consumerGroupId = "";
    kafka::Topics newSubscriptions = {};
    bool autoPoll = true;
  };

  using MessageCallback = std::function<void(
      const kafka::clients::consumer::ConsumerRecord &record)>;

  struct SubscribeOptions {
    std::vector<std::string> topics = std::vector<std::string>();
    std::string groupId = "";
    bool autoCreateTopic = true;
    std::map<std::string, std::string> extraProps = {};
    bool autoPoll = true;
    // MessageCallback callback = nullptr;
    std::vector<MessageCallback> callbacks = {};
    bool dedicatedConsumer = false;
  };

  struct SendOptions {
    std::string topic = "";
    std::map<std::string, std::string> headers = {};
    std::string value = "";
    std::function<void(const kafka::clients::producer::RecordMetadata &,
                       kafka::Error)>
        onDelivered = nullptr;
    bool autoCreateTopic = false;
  };

  // clang-format off
  /* -------------------------------------------------------------------------- */
  /*                               PUBLIC METHODS                               */
  /* -------------------------------------------------------------------------- */
  // clang-format on

  KPool(const Options &options);
  ~KPool();

  void Subscribe(const SubscribeOptions &options);

  void RegisterTopicCallback(const std::string &topic,
                             MessageCallback callback);

  void Unsubscribe(const std::string &topic, const std::string &groupId = "",
                   bool autoPoll = true);

  void Send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  void Send(const SendOptions &options);

  void CatchUp(unsigned int maxBlockingMs = 100);

  bool Poll(const std::string &groupId, unsigned int pollTimeoutMs = 100);

  void CreateTopicIfNotExists(std::string &topic, int numPartitions,
                              int replicationFactor);

  void CreateTopicsIfNotExist(std::vector<std::string> &topics,
                              int numPartitions, int replicationFactor);

  void Connect();

  void CommitSubscriptions();

  void ResetConsumers();

private:
  bool __createTopicIfNotExists(std::set<std::string> &topics,
                                int numPartitions, int replicationFactor);

  void __initAdminClient();

  void __consumerJob();

  void __send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  ubn::queue<std::function<void()>> _tasksToExecute;
  std::vector<std::shared_ptr<kafka::clients::consumer::KafkaConsumer>>
      _consumersAutoPoll;
  std::vector<std::shared_ptr<kafka::clients::consumer::KafkaConsumer>>
      _consumersManualPoll;
  std::shared_ptr<boost::mutex> _consumerMutex;
  std::atomic_bool _running;
  ubn::queue<kafka::clients::consumer::ConsumerRecord> _records;
  Options _options;
  kafka::Properties _consumerProps;
  kafka::Properties _producerProps;
  std::optional<kafka::clients::admin::AdminClient> _adminClient;
  std::optional<kafka::clients::producer::KafkaProducer> _producer;
  ubn::queue<SubscriptionTask> _subscriptionsToImplement;
  std::unordered_map<std::string, MessageCallback> _callbacks;
  boost::thread _consumerThread;
};
} // namespace nl
} // namespace celte