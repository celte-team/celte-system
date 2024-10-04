#include "MockClock.hpp"
#include "kafka/clients/consumer/ConsumerRecord.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace celte::runtime;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class ClockTest : public ::testing::Test {
protected:
  MockClock mockClock;

  kafka::clients::consumer::ConsumerRecord createConsumerRecord(int tick) {
    kafka::clients::consumer::ConsumerRecord record;
    record.value().assign(reinterpret_cast<const char *>(&tick), sizeof(tick));
    return record;
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
        mockClock.__updateCurrentTick(r);
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
      .WillOnce(Invoke([&](const kafka::clients::consumer::ConsumerRecord &r) {
        mockClock.__updateCurrentTick(r);
      }));

  mockClock.__updateCurrentTick(record);
  mockClock.CatchUp();

  EXPECT_FALSE(taskExecuted);
}

TEST_F(ClockTest, UpdateCurrentTick) {
  auto record = createConsumerRecord(10);
  EXPECT_CALL(mockClock, __updateCurrentTick(_))
      .WillOnce(Invoke([&](const kafka::clients::consumer::ConsumerRecord &r) {
        mockClock.__updateCurrentTick(r);
      }));

  mockClock.__updateCurrentTick(record);
  EXPECT_EQ(mockClock.CurrentTick(), 10);
}