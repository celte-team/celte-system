#include "Clock.hpp"
#include "Logger.hpp"
#include "Runtime.hpp"
#include <iostream>

using namespace std;
using namespace celte;

Logger &Logger::GetInstance() {
  static Logger instance;
  return instance;
}

Logger::~Logger() {
  _context->running = false;
  _context->cv.notify_all(); // Notify the thread to stop processing tasks
  if (_redisThread.joinable()) {
    _redisThread.join();
  }
  // RAII takes care of freeing the redis context ٩(^ᗜ^ )و
}

namespace detail {
static bool
createRedisContext(const std::string &host, int port,
                   std::shared_ptr<Logger::RedisThreadContext> context) {
  if (!context) {
    throw std::logic_error("Redis thread context is null");
  }
  redisContext *raw_context = redisConnect(host.c_str(), port);
  if (raw_context == NULL || raw_context->err) {
    if (raw_context) {
      std::cerr << "Redis error: " << raw_context->errstr << std::endl;
      redisFree(raw_context);
      return false;
    } else {
      std::cerr << "Can't allocate redis context" << std::endl;
      return false;
    }
  }
  context->context = std::shared_ptr<redisContext>(
      raw_context, [](redisContext *c) { redisFree(c); });
  context->running = true;
  context->uuid = RUNTIME.GetUUID();
  return true;
}

static void popTask(std::shared_ptr<Logger::RedisThreadContext> context) {
  std::unique_lock<std::mutex> lock(context->mutex);
  context->cv.wait(
      lock, [&]() { return !context->taskQueue.empty() || !context->running; });
  if (!context->running) {
    return;
  }
  Logger::RedisTask task = std::move(context->taskQueue.front());
  context->taskQueue.pop();
  lock.unlock();
  task(context->context);
}

static void
sendThreadWorker(std::shared_ptr<Logger::RedisThreadContext> context) {
  while (context->running) {
    popTask(context);
  }
}
} // namespace detail

Logger::Logger() : _context(std::make_shared<RedisThreadContext>()) {
  { // Prevent double init of singleton
    std::unique_lock<std::mutex> lock(_context->mutex);
    if (_isInit) {
      return;
    }
    _isInit = true;
  }
  int redis_port =
      std::stoi(RUNTIME.GetConfig().Get("redis_port").value_or("6379"));
  std::string redis_host =
      RUNTIME.GetConfig().Get("redis_host").value_or("localhost");
  _context->uuid = RUNTIME.GetUUID();
  _context->redisKey = RUNTIME.GetConfig().Get("redis_key").value_or("logs");

  _context = std::make_shared<RedisThreadContext>();
  if (not detail::createRedisContext(redis_host, redis_port, _context)) {
    std::cerr << "Failed to connect to redis server." << std::endl;
    exit(EXIT_FAILURE);
  }

  _context->running = true;
  _redisThread = std::thread(detail::sendThreadWorker, _context);
}

void Logger::__pushTask(const RedisTask &task) {
  std::lock_guard<std::mutex> lock(_context->mutex);
  _context->taskQueue.push(task);
  _context->cv.notify_one();
}

void Logger::Log(LogLevel level, const std::string &message) {
  std::string log_message = __getCurrentTime() + " [" +
                            __logLevelToString(level) + "] " + _context->uuid +
                            " " + message;
  std::string redisKey = _context->redisKey;
  __pushTask([log_message, redisKey](std::shared_ptr<redisContext> context) {
    redisReply *reply = (redisReply *)redisCommand(
        context.get(), "RPUSH %s %s", redisKey.c_str(), log_message.c_str());
    if (reply == NULL)
      std::cerr << "Redis send error: " << context->errstr << std::endl;
    else
      freeReplyObject(reply);
  });
}

std::string Logger::__getCurrentTime() {
  auto time = CLOCK.GetUnifiedTime();
  auto now = std::chrono::system_clock::to_time_t(time);
  char buffer[80] = {0};
  std::strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  return std::string(buffer);
}

std::string Logger::__logLevelToString(LogLevel level) {
  switch (level) {
  case DEBUG:
    return "DEBUG";
  case WARNING:
    return "WARNING";
  case ERROR:
    return "ERROR";
  case FATAL:
    return "FATAL";
  default:
    return "UNKNOWN";
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
void Logger::SetRedisKVP(const std::string &key, const std::string &value) {
  __pushTask([key, value](std::shared_ptr<redisContext> context) {
    std::string fullKey = RUNTIME.GetConfig().GetSessionId() + key;
    redisReply *reply = (redisReply *)redisCommand(
        context.get(), "SET %s %s", fullKey.c_str(), value.c_str());
    if (reply == NULL)
      std::cerr << "Redis set kvp error: " << context->errstr << std::endl;
    else
      freeReplyObject(reply);
  });
}

std::optional<std::string> Logger::GetRedisKVP(const std::string &key) {
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
  std::optional<std::string> result;

  // pushing the task to ensure that the operation is done on the same thraed
  // that is using the redis context
  __pushTask([key, &result, &done, &mutex,
              &cv](std::shared_ptr<redisContext> context) {
    std::string fullKey = RUNTIME.GetConfig().GetSessionId() + key;
    redisReply *reply =
        (redisReply *)redisCommand(context.get(), "GET %s", fullKey.c_str());
    if (reply == NULL) {
      std::cerr << "Redis get kvp error: " << context->errstr << std::endl;
      result = std::nullopt;
    } else {
      result = std::string(reply->str, reply->len);
      freeReplyObject(reply);
    }
    done = true;
    cv.notify_all();
  });
  std::unique_lock<std::mutex> lock(mutex);
  cv.wait(lock, [&done] { return done; });
  return result;
}

void Logger::GetRedisKVPAsync(const std::string &key,
                              std::function<void(bool, std::string)> callback) {
  __pushTask([key, callback, this](std::shared_ptr<redisContext> context) {
    std::optional<std::string> value = GetRedisKVP(key);
    if (value.has_value()) {
      callback(true, value.value());
    } else {
      callback(false, "");
    }
  });
}
#endif