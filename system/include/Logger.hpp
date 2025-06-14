#pragma once

#include <atomic>
#include <functional>
#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

#define LOGGER celte::Logger::GetInstance()

#ifndef RELEASE
#define LOGDEBUG(message)                                                      \
  celte::Logger::GetInstance().Log(celte::Logger::LogLevel::DEBUG, message)
#define LOGINFO(message)                                                       \
  celte::Logger::GetInstance().Log(celte::Logger::LogLevel::DEBUG, message)
#else
#define LOGDEBUG(message)
#define LOGINFO(message)
#endif
#define LOGWARNING(message)                                                    \
  celte::Logger::GetInstance().Log(celte::Logger::LogLevel::WARNING, message)
#define LOGERROR(message)                                                      \
  celte::Logger::GetInstance().Log(celte::Logger::LogLevel::ERROR, message)

namespace celte {

class Logger {
public:
  static Logger &GetInstance();
  enum LogLevel { DEBUG, WARNING, ERROR, FATAL };

private: // Singleton instance
  Logger(const Logger &) = delete;
  Logger();
  ~Logger();

public:
  void Log(LogLevel level, const std::string &message);

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

  using RedisTask = std::function<void(std::shared_ptr<redisContext>)>;

  struct RedisThreadContext {
    std::shared_ptr<redisContext> context;
    std::atomic_bool running;
    std::string uuid;
    std::string redisKey;
    std::queue<RedisTask> taskQueue;
    std::mutex mutex;
    std::condition_variable cv;
  };

private:
  std::string __logLevelToString(LogLevel level);
  std::string __getCurrentTime();

  void __pushTask(const RedisTask &task);

  std::shared_ptr<RedisThreadContext> _context; // Used for redis operations
  std::thread _redisThread; // Thread for processing redis tasks
  std::atomic_bool _isInit = false;
};

} // namespace celte
