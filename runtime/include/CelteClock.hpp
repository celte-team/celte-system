#pragma once

#include <functional>
#include <kafka/KafkaConsumer.h>
#include <queue>
#include <vector>

namespace celte {
namespace runtime {

/**
 * @brief This class will be used to keep track of the time in the game,
 * and synchronize events between peers.
 *
 * The time is diffused through kafka in the 'global.clock' topic.
 * Time is represented as an integer representing how many Ticks have passed.
 * Ticks are used instead of real time because it allows for time scaling
 * to adapt the system to the game's needs.
 */
class Clock {
public:
  /**
   * @brief Initializes the clock (subscribes to kafka).
   * Note that the initial connection to kafka must have
   * been established before calling this method or it will fail.
   */
  void Init();

  /**
   * @brief Schedules a task to be executed at a given tick.
   * If the tick is in the past, the task will be executed immediately.
   */
  void ScheduleAt(int tick, std::function<void()> task);

  /**
   * @brief Schedules a task to be executed after a given number of ticks.
   * The task will be executed at the current tick + delay.
   */
  inline void ScheduleAfter(int delay, std::function<void()> task) {
    ScheduleAt(_tick + delay, task);
  }

  /**
   * @brief Returns the current tick of the clock.
   * This is the number of ticks that have passed since the clock was started.
   */
  inline int CurrentTick() const { return _tick; }

  /**
   * @brief Advance the task queue until no more tasks are due by the current
   * tick.
   */
  void CatchUp();

private:
  /**
   * @brief Updates the current tick of the clock.
   * This method is called when a message is received on the
   * 'global.clock' topic.
   *
   * @param r The record containing the tick.
   */
  void __updateCurrentTick(const kafka::clients::consumer::ConsumerRecord &r);

  // Comparator to order tasks by tick in ascending order
  struct TaskComparator {
    bool operator()(const std::pair<int, std::function<void()>> &lhs,
                    const std::pair<int, std::function<void()>> &rhs) const {
      return lhs.first >
             rhs.first; // Min-heap behavior (earlier tick has higher priority)
    }
  };

  // Priority queue with custom comparator
  std::priority_queue<std::pair<int, std::function<void()>>,
                      std::vector<std::pair<int, std::function<void()>>>,
                      TaskComparator>
      _tasks;

  int _tick = 0;
};

} // namespace runtime
} // namespace celte