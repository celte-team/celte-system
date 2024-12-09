#include "Replicator.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <msgpack.hpp>

using namespace celte::runtime;

class ReplicatorTest : public ::testing::Test {
protected:
  Replicator replicator;

  void SetUp() override {
    // replicator.registerValue("key1", &data1);
    // replicator.registerValue("key2", &data2);
    replicator.RegisterReplicatedValue(
        "key1", [this]() -> std::string { return std::to_string(data1); },
        [this](const std::string &data) { data1 = std::stoi(data); });
    replicator.RegisterReplicatedValue(
        "key2", [this]() -> std::string { return data2; },
        [this](const std::string &data) { data2 = data; });
  }

public:
  int data1 = 42;
  std::string data2 = "hello";
};

TEST_F(ReplicatorTest, GetBlob) {
  data1++;
  Replicator::ReplBlob blob = replicator.GetBlob();

  EXPECT_FALSE(blob.empty());
}

TEST_F(ReplicatorTest, Overwrite) {
  Replicator::ReplBlob blob = replicator.GetBlob();
  data1++;
  replicator.Overwrite(blob);

  EXPECT_EQ(data1, 42);
}