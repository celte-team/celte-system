#pragma once
#include <tbb/concurrent_queue.h>

namespace celte {
/// @brief This class serves as an endpoint for the engine to push tasks to be
/// ran in the system, and for the system to push tasks to be ran in the
/// engine's context.
class Executor {
public:
  /// @brief Pushes a task that will be executed asynchronously by the system.
  /// @param task
  void PushTaskToSystem(std::function<void()> task);

  /// @brief Pushes a task that will be executed asynchronously by the system.
  /// This task is optimized for I/O tasks.
  void PushIOTaskToSystem(std::function<void()> task);

  /// @brief Pushes a task that will be executed in the engine's context.
  inline void PushTaskToEngine(std::function<void()> task) {
    _engineTasks.push(task);
  }

  /// @brief Returns the next task in the engine queue, if any.
  std::optional<std::function<void()>> PollEngineTask();

private:
  tbb::concurrent_queue<std::function<void()>> _engineTasks;
};
} // namespace celte