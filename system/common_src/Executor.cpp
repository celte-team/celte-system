#include "Executor.hpp"
#include "Runtime.hpp"
using namespace celte;

std::optional<std::function<void()>> Executor::PollEngineTask() {
  std::function<void()> task;
  if (_engineTasks.try_pop(task)) {
    return task;
  }
  return std::nullopt;
}

void Executor::PushTaskToSystem(std::function<void()> task) {
  RUNTIME.ScheduleAsyncTask(task);
}

void Executor::PushIOTaskToSystem(std::function<void()> task) {
  RUNTIME.ScheduleAsyncIOTask(task);
}