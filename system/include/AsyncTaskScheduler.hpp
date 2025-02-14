#pragma once
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <tbb/task_arena.h>
#include <tbb/task_group.h>

namespace celte {
/// @brief A class that schedules tasks to be executed asynchronously.
/// @warning Do not use this class to schedule tasks that are not thread-safe,
/// or that have side effects in the engine.
class AsyncTaskScheduler {
public:
  /// @brief Constructs the scheduler with a specified number of threads.
  /// @param numThreads Number of threads in the arena. Use -1 for automatic.
  explicit AsyncTaskScheduler(int numThreads = -1);

  ~AsyncTaskScheduler();

  /// @brief Schedules a task to be executed.
  /// @param task A callable object representing the task.
  void Schedule(std::function<void()> task);

  /// @brief Waits for all tasks to complete.
  void Wait();

private:
  tbb::task_arena arena; ///< Manages the thread pool and its size.
  tbb::task_group group; ///< Executes and manages scheduled tasks.
};

class AsyncIOTaskScheduler {
public:
  explicit AsyncIOTaskScheduler(int numThreads = -1);

  ~AsyncIOTaskScheduler();

  void Schedule(std::function<void()> task);

  void Wait();

private:
  boost::asio::io_service _ioService;
  boost::asio::io_service::work _work;
  boost::thread_group _threads;
};
} // namespace celte