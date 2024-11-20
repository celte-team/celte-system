// just a file to try to find under what conditions does kafka send this signal:
// / Users / eliotjanvier / vcpkg / buildtrees / librdkafka / src / v2 .3.0 -
//         ddad008325.clean / src /
//             rdkafka_queue.c : 581 : rd_kafka_q_serve : assert : res !=
//     RD_KAFKA_OP_RES_PASS ***zsh : abort

#include "KPool.hpp"
#include "kafka/AdminClient.h"
#include "kafka/KafkaConsumer.h"
#include "kafka/KafkaProducer.h"
#include <chrono>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <set>

TEST(KPoolV2, CreateAndSubscribeToLists) {
  celte::nl::KPool kpool({
      .bootstrapServers = "localhost:80",
  });

  kpool.Connect();

  kpool.Subscribe({
      .topics = {"topic1", "topic2"},
      .autoCreateTopic = true,
  });

  kpool.Subscribe({
      .topics = {"topic3", "topic4"},
      .autoCreateTopic = true,
  });

  kpool.Subscribe({
      .topics = {"topic5", "topic6", "topic7", "topic8", "topic9"},
      .autoCreateTopic = true,
  });

  kpool.CommitSubscriptions();

  kpool.Subscribe({
      .topics = {"topic10", "topic11", "topic12"},
      .autoCreateTopic = true,
  });

  kpool.CommitSubscriptions();
}