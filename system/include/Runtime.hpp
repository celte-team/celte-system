#pragma once
#include "AsyncTaskScheduler.hpp"
#include "ETTRegistry.hpp"
#include <string>
#include <tbb/concurrent_queue.h>

#define RUNTIME celte::Runtime::GetInstance()

namespace celte {
class Runtime {
public:
  static Runtime &GetInstance();
  Runtime();

  /* ---------------------- FUNCTIONS EXPOSED TO THE API ----------------------
   */

  /// @brief Connects to the cluster using environment variables.
  /// CELTE_HOST for the address. Port is the default pulsar port (6650).
  /// @note This function is blocking.
  void ConnectToCluster();

  /// @brief Connects to the cluster using the provided address and port.
  /// @param address The address of the cluster.
  /// @param port The port of the cluster.
  /// @note This function is blocking.
  void ConnectToCluster(const std::string &address, int port);

  /// @brief Executes the runtime loop once. Call this once per frame in the
  /// engine.
  void Tick();

  /* ---------------------- FUNCTIONS FOR INTERNAL USE ---------------------- */

  /// @brief Registers a task that will run in the same thread as Runtime::Tick.
  /// @param task
  inline void ScheduleSyncTask(std::function<void()> task) {
    _syncTasks.push(task);
  }

  /// @brief Registers a task that will run in a separate thread. For I/O tasks,
  /// use ScheduleAsyncIOTask instead.
  /// @param task
  inline void ScheduleAsyncTask(std::function<void()> task) {
    _asyncScheduler.Schedule(task);
  }

  /// @brief Registers a task that will run in a separate thread. Optimized for
  /// I/O tasks.
  inline void ScheduleAsyncIOTask(std::function<void()> task) {
    _asyncIOTaskScheduler.Schedule(task);
  }

  /// @brief Returns the AsyncTaskScheduler instance.
  inline AsyncTaskScheduler &GetAsyncTaskScheduler() { return _asyncScheduler; }

  /// @brief Returns the AsyncIOTaskScheduler instance.
  inline AsyncIOTaskScheduler &GetAsyncIOTaskScheduler() {
    return _asyncIOTaskScheduler;
  }

  /// @brief Returns the unique identifier of this peer on the network.
  inline const std::string &GetUUID() const { return _uuid; }

private:
  tbb::concurrent_queue<std::function<void()>> _syncTasks;
  AsyncTaskScheduler _asyncScheduler;
  AsyncIOTaskScheduler _asyncIOTaskScheduler;
  const std::string _uuid;
};
} // namespace celte
