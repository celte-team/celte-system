#include "KPool.hpp"
#include "Replicator.hpp"
#include <chrono>
#include <iostream>
#include <kafka/AdminClient.h>
#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>
#include <kafka/Properties.h>
#include <set>

static int totalElapsedTime = 0;
static int numCalls = 0;
static std::chrono::time_point<std::chrono::system_clock> start;

static void startMeasuringTime() {
  totalElapsedTime = 0;
  start = std::chrono::system_clock::now();
  ++numCalls;
}

static void stopMeasuringTime(const std::string &message) {
  totalElapsedTime += std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now() - start)
                          .count();
  std::cout << message << totalElapsedTime / numCalls << "ms\n";
}

static void resetMeasures() {
  totalElapsedTime = 0;
  numCalls = 0;
}

int main() {
  // benchmark the time it takes to create a topic using the adminclient.
  kafka::Properties props;
  props.put("bootstrap.servers", "localhost:80");
  props.put("log_level", "3");
  kafka::clients::admin::AdminClient adminClient(props);
  std::string topic = "test";
  int numPartitions = 1;
  int replicationFactor = 1;
  std::chrono::time_point<std::chrono::system_clock> start, end;
  start = std::chrono::system_clock::now();
  for (int i = 0; i < 100; i++) {
    adminClient.createTopics({topic + std::to_string(i)}, numPartitions,
                             replicationFactor);
  }
  end = std::chrono::system_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  std::cout << "Time to create 100 topics: " << elapsed_seconds.count()
            << "s\n";
  std::cout << "average time to create a topic: "
            << elapsed_seconds.count() / 100 << "s\n";

  // benchmark the time it takes to subscribe to a topic using the kpool.
  celte::nl::KPool::Options options;
  options.bootstrapServers = "localhost:80";
  celte::nl::KPool kpool(options);
  kpool.Connect();
  std::cout << "KPool connected" << std::endl;

  // subscribe in baches of 10
  for (int i = 0; i < 10; ++i) {
    celte::nl::KPool::SubscribeOptions subscribeOptions;
    std::atomic_bool ok = false;
    for (int j = 0; j < 10; ++j) {
      subscribeOptions.topics.push_back("topic" + std::to_string((i + 1) * j));
    }
    subscribeOptions.then = [&ok] { ok = true; };
    startMeasuringTime();
    kpool.Subscribe(subscribeOptions);
    kpool.CommitSubscriptions();
    while (!ok)
      kpool.CatchUp();
    stopMeasuringTime("Average time to subscribe to 10 topics: ");
  }
}