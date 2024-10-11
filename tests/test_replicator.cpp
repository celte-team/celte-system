#include "Replicator.hpp"
#include <cstring>
#include <gtest/gtest.h>
#include <msgpack.hpp>

using namespace celte::runtime;

class ReplicatorTest : public ::testing::Test {
protected:
  Replicator replicator;

  void SetUp() override {
    replicator.registerValue("key1", data1);
    replicator.registerValue("key2", data2);
  }

public:
  int data1 = 42;
  std::string data2 = "hello";
};

TEST_F(ReplicatorTest, NotifyDataChanged) {
  replicator.notifyDataChanged("key1");
  EXPECT_TRUE(replicator.getValue<int>("key1") == 42);
}

TEST_F(ReplicatorTest, GetBlob) {
  replicator.notifyDataChanged("key1");
  Replicator::ReplBlob blob = replicator.GetBlob();

  EXPECT_FALSE(blob.data.empty());
}

TEST_F(ReplicatorTest, CorrectDataInBlob) {
  data1 = 100;
  replicator.notifyDataChanged("key1");
  Replicator::ReplBlob blob = replicator.GetBlob();

  msgpack::unpacker unpacker;
  unpacker.reserve_buffer(blob.data.size());
  std::memcpy(unpacker.buffer(), blob.data.data(), blob.data.size());
  unpacker.buffer_consumed(blob.data.size());

  msgpack::object_handle oh;
  unpacker.next(oh);
  msgpack::object obj = oh.get();
  std::string key;
  size_t dataSize;
  msgpack::type::raw_ref rawData;

  obj.convert(key); // Unpack the key
  unpacker.next(oh);
  oh.get().convert(dataSize); // Unpack the size of the data
  unpacker.next(oh);
  oh.get().convert(rawData); // Unpack the data

  EXPECT_EQ(key, "key1");
  EXPECT_EQ(dataSize, sizeof(int));
  EXPECT_EQ(*reinterpret_cast<const int *>(rawData.ptr), 100);

  // Ensure that the data is not changed in the replicator
  EXPECT_EQ(replicator.getValue<int>("key1"), 100);
}

TEST_F(ReplicatorTest, InvalidRegister) {
  replicator.notifyDataChanged("key1");
  Replicator::ReplBlob blob = replicator.GetBlob();

  int newData = 100;
  EXPECT_THROW(
      { replicator.registerValue("key1", newData); }, std::runtime_error);
}

TEST_F(ReplicatorTest, ResetDataChanged) {
  replicator.notifyDataChanged("key1");
  replicator.notifyDataChanged("key2");

  replicator.resetDataChanged();

  // Check if the hasChanged flag is reset (indirectly by checking GetBlob)
  Replicator::ReplBlob blob = replicator.GetBlob();
  EXPECT_TRUE(blob.data.empty());
}

// Edge case: No data changed
TEST_F(ReplicatorTest, GetBlobNoDataChanged) {
  Replicator::ReplBlob blob = replicator.GetBlob();

  EXPECT_TRUE(blob.data.empty());
}

// Edge case: Overwrite with empty blob
TEST_F(ReplicatorTest, OverwriteWithEmptyBlob) {
  Replicator::ReplBlob emptyBlob;
  replicator.Overwrite(emptyBlob);

  // Ensure no data was changed
  EXPECT_EQ(replicator.getValue<int>("key1"), 42);
  EXPECT_EQ(replicator.getValue<std::string>("key2"), "hello");
}

// Edge case: Notify data changed for non-existent key
TEST_F(ReplicatorTest, NotifyDataChangedNonExistentKey) {
  replicator.notifyDataChanged("non_existent_key");

  // Ensure no data was changed
  EXPECT_EQ(replicator.getValue<int>("key1"), 42);
  EXPECT_EQ(replicator.getValue<std::string>("key2"), "hello");
}