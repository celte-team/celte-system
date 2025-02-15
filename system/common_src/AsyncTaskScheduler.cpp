#include "AsyncTaskScheduler.hpp"
#include <stdexcept>

namespace celte {

/* ------------------------------ Compute tasks ----------------------------- */

AsyncTaskScheduler::AsyncTaskScheduler(int numThreads)
    : arena(numThreads == -1 ? tbb::task_arena::automatic : numThreads) {
  arena.initialize();
}

AsyncTaskScheduler::~AsyncTaskScheduler() { Wait(); }

void AsyncTaskScheduler::Schedule(std::function<void()> task) {
  arena.execute(
      [this, task = std::move(task)]() { group.run(std::move(task)); });
}

void AsyncTaskScheduler::Wait() { group.wait(); }

/* -------------------------------- IO Tasks -------------------------------- */

AsyncIOTaskScheduler::AsyncIOTaskScheduler(int numThreads) : _work(_ioService) {
  if (numThreads == -1) {
    numThreads = std::thread::hardware_concurrency();
  }
  for (int i = 0; i < numThreads; i++) {
    _threads.create_thread([this]() { _ioService.run(); });
  }
}

AsyncIOTaskScheduler::~AsyncIOTaskScheduler() { Wait(); }

void AsyncIOTaskScheduler::Schedule(std::function<void()> task) {
  _ioService.post(std::move(task));
}

void AsyncIOTaskScheduler::Wait() {
  _ioService.stop();
  _threads.join_all();
}

} // namespace celte