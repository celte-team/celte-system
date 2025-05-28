#include <atomic>
#include <functional>
#include <iostream>
#include <pulsar/Client.h>
#include <random>
#include <thread>
#include <vector>

static pulsar::Client *client = nullptr;

int main() {

  pulsar::ClientConfiguration config;
  client = new pulsar::Client("pulsar://localhost:6650", config);

  const int numProducers = 100;
  const int numConsumers = 100;
  const int minTopic = 100;
  const int maxTopic = 1000;

  // Helper to generate random topic numbers
  auto getRandomTopic = []() -> int {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(minTopic, maxTopic);
    return dist(gen);
  };

  std::vector<std::thread> producerThreads;
  std::vector<std::thread> consumerThreads;
  std::vector<pulsar::Producer *> producers(numProducers, nullptr);
  std::vector<pulsar::Consumer *> consumers(numConsumers, nullptr);
  std::atomic<bool> running{true};

  // Start producers
  for (int i = 0; i < numProducers; ++i) {
    producerThreads.emplace_back([i, &producers, &getRandomTopic]() {
      std::string topic = "topic-" + std::to_string(getRandomTopic());
      pulsar::ProducerConfiguration prodConfig;
      pulsar::Producer *producer = new pulsar::Producer;
      if (client->createProducer(topic, prodConfig, *producer) !=
          pulsar::ResultOk) {
        std::cerr << "Failed to create producer for " << topic << std::endl;
        delete producer;
        return;
      }
      producers[i] = producer;
      while (true) {
          std::shared_ptr<std::string> oui = std::make_shared<std::string>(
              "Hello from producer " + std::to_string(i) + " to topic " + topic);
          auto msg = pulsar::MessageBuilder().setContent(*oui.get());
          producers[i]->sendAsync(
              msg.build(),
              [i, topic, oui](pulsar::Result result,
                               const pulsar::MessageId &messageId) {
          if (result != pulsar::ResultOk) {
            std::cerr << "Failed to send message from producer " << i
                      << " to topic " << topic << std::endl;
            std::cerr << "Error: " << pulsar::strResult(result) << std::endl;
          } else {
            // std::cout << "Producer " << i << " sent message to topic " << topic
            //           << std::endl;
          }
          });
          std::this_thread::sleep_for(
              std::chrono::milliseconds(100)); // Throttle
        }
      });
  }

  // Start consumers
  for (int i = 0; i < numConsumers; ++i) {
      consumerThreads.emplace_back([i, &consumers, &getRandomTopic,
                                    &running]() {
        std::string topic = "topic-" + std::to_string(getRandomTopic());
        std::string subName = "sub-" + std::to_string(i);
        pulsar::ConsumerConfiguration consConfig;
        consConfig.setMessageListener(
            [i](pulsar::Consumer &consumer, const pulsar::Message &msg) {
              consumer.acknowledge(msg);
            });
        client->subscribeAsync(
            topic, subName, consConfig,
            [i, &consumers](pulsar::Result result, pulsar::Consumer consumer) {
              if (result != pulsar::ResultOk) {
                std::cerr << "Failed to subscribe consumer " << i << std::endl;
                return;
              }
              consumers[i] = new pulsar::Consumer(std::move(consumer));
            });
      });
  }

  // Join all threads
  for (auto &t : producerThreads)
    t.join();
  for (auto &t : consumerThreads)
    t.join();

  // Cleanup
  for (auto *producer : producers) {
      if (producer) {
        producer->close();
        delete producer;
      }
  }
  delete client;
  return 0;
  }
