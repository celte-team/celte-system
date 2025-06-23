#include "AsyncTaskScheduler.hpp"
#include <stdexcept>
#ifdef CELTE_SERVER_MODE_ENABLED
#include "MetricsScrapper.hpp"
#endif
#include <atomic>
#include <functional>

namespace celte {

#ifdef CELTE_SERVER_MODE_ENABLED
#include <chrono>
namespace {
static std::atomic<int> s_pendingComputeTasks{0};
static std::atomic<std::uint64_t> s_totalWaitTimeNs{0};
static std::atomic<std::uint64_t> s_totalWaitCount{0};
} // namespace
#endif

/* ------------------------------ Compute tasks ----------------------------- */

AsyncTaskScheduler::AsyncTaskScheduler(int numThreads)
    : arena(numThreads == -1 ? tbb::task_arena::automatic : numThreads) {
  arena.initialize();
#ifdef CELTE_SERVER_MODE_ENABLED
  celte::MetricsScrapper::GetInstance().RegisterMetric<int>(
      "pending_compute_tasks", []() { return s_pendingComputeTasks.load(); });
  celte::MetricsScrapper::GetInstance().RegisterMetric<double>(
      "avg_compute_task_wait_time_ms", []() {
        auto count = s_totalWaitCount.load();
        if (count == 0)
          return 0.0;
        return static_cast<double>(s_totalWaitTimeNs.load()) / count / 1e6;
      });
#endif
}

AsyncTaskScheduler::~AsyncTaskScheduler() { Wait(); }

void AsyncTaskScheduler::Schedule(std::function<void()> task) {
#ifdef CELTE_SERVER_MODE_ENABLED
  s_pendingComputeTasks.fetch_add(1, std::memory_order_relaxed);
  auto schedule_time = std::chrono::steady_clock::now();
#endif
  arena.execute([this, task = std::move(task)
#ifdef CELTE_SERVER_MODE_ENABLED
                           ,
                 schedule_time
#endif
  ]() {
    try {
      group.run([task = std::move(task)
#ifdef CELTE_SERVER_MODE_ENABLED
                     ,
                 schedule_time
#endif
      ]() {
#ifdef CELTE_SERVER_MODE_ENABLED
        auto start_time = std::chrono::steady_clock::now();
        auto wait_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           start_time - schedule_time)
                           .count();
        s_totalWaitTimeNs.fetch_add(wait_ns, std::memory_order_relaxed);
        s_totalWaitCount.fetch_add(1, std::memory_order_relaxed);
#endif
        try {
          task();
        } catch (...) {
        }
#ifdef CELTE_SERVER_MODE_ENABLED
        s_pendingComputeTasks.fetch_sub(1, std::memory_order_relaxed);
#endif
      });
    } catch (...) {
#ifdef CELTE_SERVER_MODE_ENABLED
      s_pendingComputeTasks.fetch_sub(1, std::memory_order_relaxed);
#endif
    }
  });
}

void AsyncTaskScheduler::Wait() { group.wait(); }

/* -------------------------------- IO Tasks -------------------------------- */
#ifdef CELTE_SERVER_MODE_ENABLED
namespace {
static std::atomic<int> s_pendingIOTasks{0};
static std::atomic<std::uint64_t> s_totalIOWaitTimeNs{0};
static std::atomic<std::uint64_t> s_totalIOWaitCount{0};
} // namespace
#endif

AsyncIOTaskScheduler::AsyncIOTaskScheduler(int numThreads) : _work(_ioService) {
  if (numThreads == -1) {
    numThreads = std::thread::hardware_concurrency();
  }
  for (int i = 0; i < numThreads; i++) {
    _threads.create_thread([this]() { _ioService.run(); });
  }
#ifdef CELTE_SERVER_MODE_ENABLED
  celte::MetricsScrapper::GetInstance().RegisterMetric<int>(
      "pending_io_tasks", []() { return s_pendingIOTasks.load(); });
#endif
#ifdef CELTE_SERVER_MODE_ENABLED
  celte::MetricsScrapper::GetInstance().RegisterMetric<double>(
      "avg_io_task_wait_time_ms", []() {
        auto count = s_totalIOWaitCount.load();
        if (count == 0)
          return 0.0;
        return static_cast<double>(s_totalIOWaitTimeNs.load()) / count / 1e6;
      });
#endif
}

AsyncIOTaskScheduler::~AsyncIOTaskScheduler() { Wait(); }
void AsyncIOTaskScheduler::Schedule(std::function<void()> task) {
#ifdef CELTE_SERVER_MODE_ENABLED
  s_pendingIOTasks.fetch_add(1, std::memory_order_relaxed);
  auto schedule_time = std::chrono::steady_clock::now();
#endif
  _ioService.post([task = std::move(task)
#ifdef CELTE_SERVER_MODE_ENABLED
                       ,
                   schedule_time
#endif
  ]() mutable {
#ifdef CELTE_SERVER_MODE_ENABLED
    auto start_time = std::chrono::steady_clock::now();
    auto wait_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       start_time - schedule_time)
                       .count();
    s_totalIOWaitTimeNs.fetch_add(wait_ns, std::memory_order_relaxed);
    s_totalIOWaitCount.fetch_add(1, std::memory_order_relaxed);
#endif
    try {
      task();
    } catch (...) {
    }
#ifdef CELTE_SERVER_MODE_ENABLED
    s_pendingIOTasks.fetch_sub(1, std::memory_order_relaxed);
#endif
  });
}

void AsyncIOTaskScheduler::Wait() {
  _ioService.stop();
  _threads.join_all();
}

} // namespace celte