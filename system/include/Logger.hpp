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

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Stores a key value pair to redis. The session Id is automatically
  /// added to the key, so that the key is unique to this game session.
  /// @param key
  /// @param value
  void SetRedisKVP(const std::string &key, const std::string &value);

  /// @brief Returns the value associated with the key in redis, or std::nullopt
  /// if not found.
  /// @param key
  /// @return
  std::optional<std::string> GetRedisKVP(const std::string &key);

  /// @brief Asynchronously retrieves a value from a key from redis.
  /// @param key
  /// @param callback : function <void(bool, std::string)> where the first
  /// parameter is true if the key was found, and the second parameter is the
  /// value.
  void GetRedisKVPAsync(const std::string &key,
                        std::function<void(bool, std::string)> callback);
#endif

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
