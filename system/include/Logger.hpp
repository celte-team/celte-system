#pragma once

#include <atomic>
#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <string>
#include <tbb/concurrent_queue.h>
#include <thread>

#define LOGGER celte::Logger::GetInstance()

#ifndef RELEASE
#define LOGDEBUG(message)                                                      \
  celte::Logger::GetInstance().log(celte::Logger::LogLevel::DEBUG, message)
#define LOGINFO(message)                                                       \
  celte::Logger::GetInstance().log(celte::Logger::LogLevel::DEBUG, message)
#else
#define LOGDEBUG(message)
#define LOGINFO(message)
#endif
#define LOGWARNING(message)                                                    \
  celte::Logger::GetInstance().log(celte::Logger::LogLevel::WARNING, message)
#define LOGERROR(message)                                                      \
  celte::Logger::GetInstance().log(celte::Logger::LogLevel::ERROR, message)

namespace celte {

class Logger {
public:
  static Logger &GetInstance();
  enum LogLevel { DEBUG, WARNING, ERROR, FATAL };

  Logger();
  ~Logger();

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
