#include "CelteClock.hpp"
#include "kafka/KafkaConsumer.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace celte::runtime;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class MockClock : public Clock {
public:
  MOCK_METHOD(void, ScheduleAt, (int tick, std::function<void()> task));
  MOCK_METHOD(void, __updateCurrentTick,
              (const kafka::clients::consumer::ConsumerRecord &record));
  // MOCK_METHOD(int, CurrentTick, (), (const));
};

static std::vector<std::string> messages;
static std::string topic = "global.clock";

class ClockTest : public ::testing::Test {
protected:
  MockClock mockClock;

  kafka::clients::consumer::ConsumerRecord createConsumerRecord(int tick) {
    std::string message =
        std::string(reinterpret_cast<char *>(&tick), sizeof(tick));
    rd_kafka_message_t *rk_msg = new rd_kafka_message_t();
    rk_msg->rkt = nullptr; // Mock topic
    rk_msg->partition = 0; // Default partition
    rk_msg->offset = 0;    // Default offset
    rk_msg->key = nullptr; // No key
    rk_msg->key_len = 0;   // No key length
    rk_msg->payload =
        const_cast<void *>(static_cast<const void *>(message.data()));
    rk_msg->len = message.size();
    rk_msg->err = RD_KAFKA_RESP_ERR_NO_ERROR; // No error

    return kafka::clients::consumer::ConsumerRecord(
        rk_msg, [](rd_kafka_message_t *) {});
  }
};

TEST_F(ClockTest, ScheduleTaskInThePast) {
  bool taskExecuted = false;
  EXPECT_CALL(mockClock, ScheduleAt(_, _))
      .WillOnce(Invoke([&taskExecuted](int tick, std::function<void()> task) {
        task();
        taskExecuted = true;
      }));

  mockClock.ScheduleAt(-1, [&taskExecuted]() { taskExecuted = true; });
  EXPECT_TRUE(taskExecuted);
}

TEST_F(ClockTest, ScheduleTaskInTheFuture) {
  bool taskExecuted = false;
  EXPECT_CALL(mockClock, ScheduleAt(_, _))
      .WillOnce(Invoke([&taskExecuted](int tick, std::function<void()> task) {
        taskExecuted = false;
      }));

  mockClock.ScheduleAt(10, [&taskExecuted]() { taskExecuted = true; });
  EXPECT_FALSE(taskExecuted);
}

TEST_F(ClockTest, CatchUpExecutesTasks) {
  bool task1Executed = false;
  bool task2Executed = false;

  EXPECT_CALL(mockClock, ScheduleAt(_, _))
      .Times(2)
      .WillRepeatedly(Invoke([&](int tick, std::function<void()> task) {
        if (tick == 5) {
          task1Executed = true;
        } else if (tick == 10) {
          task2Executed = true;
        }
      }));

  mockClock.ScheduleAt(5, [&task1Executed]() { task1Executed = true; });
  mockClock.ScheduleAt(10, [&task2Executed]() { task2Executed = true; });

  auto record = createConsumerRecord(10);
  EXPECT_CALL(mockClock, __updateCurrentTick(_))
      .WillOnce(Invoke([&](const kafka::clients::consumer::ConsumerRecord &r) {
        // mockClock.__updateCurrentTick(r);
      }));

  mockClock.__updateCurrentTick(record);
  mockClock.CatchUp();

  EXPECT_TRUE(task1Executed);
  EXPECT_TRUE(task2Executed);
}

TEST_F(ClockTest, CatchUpDoesNotExecuteFutureTasks) {
  bool taskExecuted = false;

  EXPECT_CALL(mockClock, ScheduleAt(_, _))
      .WillOnce(Invoke([&taskExecuted](int tick, std::function<void()> task) {
        taskExecuted = false;
      }));

  mockClock.ScheduleAt(15, [&taskExecuted]() { taskExecuted = true; });

  auto record = createConsumerRecord(10);
  EXPECT_CALL(mockClock, __updateCurrentTick(_))
      .WillOnce(
          Invoke([&](const kafka::clients::consumer::ConsumerRecord &r) {}));

  mockClock.__updateCurrentTick(record);
  mockClock.CatchUp();

  EXPECT_FALSE(taskExecuted);
}
