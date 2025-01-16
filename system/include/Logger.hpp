/*
** CELTE, 2025
** celte-system

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** Logger
*/

#pragma once

#include <atomic>
#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <string>
#include <tbb/concurrent_queue.h>
#include <thread>

#define LOGGER celte::Logger::GetInstance()

namespace celte {

class Logger {
public:
  static Logger &GetInstance();
  enum LogLevel { DEBUG, WARNING, ERROR, FATAL };

  // uuid should be the RUNTIME.currentUUID
  // key can be the pid to split logs by process, OR common to all process to
  // centrilize them
  Logger();
  ~Logger();

  //  log should be async, add a function to a pool to not slow the process
  void log(LogLevel level, const std::string &message);

private:
  std::string logLevelToString(LogLevel level);
  std::string getCurrentTime();
  void __sendThreadWorker();

  std::shared_ptr<redisContext> context;
  std::mutex log_mutex;
  std::string redis_key;
  std::string uuid;
  tbb::concurrent_queue<std::string> log_queue;
  std::thread log_thread;
  std::atomic_bool log_thread_running;
  bool output_console = false;
};

} // namespace celte
