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

Logger::Logger() {
  int redis_port = std::stoi(RUNTIME.GetConfig().Get("redis_port").value());
  std::string redis_host = RUNTIME.GetConfig().Get("redis_host").value();
  std::string key = RUNTIME.GetConfig().Get("redis_key").value();
  std::string uuid = RUNTIME.GetUUID();
  redis_key = key;
  this->uuid = uuid;

  redisContext *raw_context = redisConnect(redis_host.c_str(), redis_port);
  if (raw_context == NULL || raw_context->err) {
    if (raw_context) {
      std::cerr << "Error: " << raw_context->errstr << std::endl;
      redisFree(raw_context);
    } else {
      std::cerr << "Can't allocate redis context" << std::endl;
    }
    // throw std::runtime_error("Failed to connect to Redis");
    std::cerr
        << "Failed to connect to Redis logs to redis, falling back to stdout"
        << std::endl;
    output_console = true;
  }
  context = std::shared_ptr<redisContext>(
      raw_context, [](redisContext *c) { redisFree(c); });

  log_thread_running = true;
  log_thread = std::thread(&Logger::__sendThreadWorker, this);
}

Logger::~Logger() {
  log_thread_running = false;
  if (log_thread.joinable()) {
    log_thread.join();
  }
}

void Logger::log(LogLevel level, const std::string &message) {
  std::string log_message = getCurrentTime() + " [" + logLevelToString(level) +
                            "] " + uuid + " " + message;
  log_queue.push(log_message);
}

std::string Logger::getCurrentTime() {
  auto time = CLOCK.GetUnifiedTime();
  auto now = std::chrono::system_clock::to_time_t(time);
  char buffer[80];
  std::strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", std::localtime(&now));

  return std::string(buffer);
}

std::string Logger::logLevelToString(LogLevel level) {
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

void Logger::__sendThreadWorker() {
  while (log_thread_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::string message;
    if (log_queue.try_pop(message)) {
      if (output_console) {
        std::cout << message << std::endl;
        continue;
      }

      redisReply *reply = (redisReply *)redisCommand(
          context.get(), "RPUSH %s %s", redis_key.c_str(), message.c_str());

      if (reply == NULL)
        std::cerr << "Error: " << context->errstr << std::endl;
      else
        freeReplyObject(reply);
    }
  }
}