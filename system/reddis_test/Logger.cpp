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

#include "Logger.hpp"
#include <iostream>

Logger::Logger(const std::string& redis_host, int redis_port, const std::string& key, const std::string& uuid)
    : redis_key(key)
    , uuid(uuid)
{
    redisContext* raw_context = redisConnect(redis_host.c_str(), redis_port);
    if (raw_context == NULL || raw_context->err) {
        if (raw_context) {
            std::cerr << "Error: " << raw_context->errstr << std::endl;
            redisFree(raw_context);
        } else {
            std::cerr << "Can't allocate redis context" << std::endl;
        }
        throw std::runtime_error("Failed to connect to Redis");
    }
    context = std::shared_ptr<redisContext>(raw_context, [](redisContext* c) {
        redisFree(c);
    });
}

Logger::~Logger()
{
}

void Logger::log(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string log_message = getCurrentTime() + " - " + uuid + " - " + logLevelToString(level) + ": " + message;
    redisReply* reply = (redisReply*)redisCommand(context.get(), "RPUSH %s %s", redis_key.c_str(), log_message.c_str());

    if (reply == NULL)
        std::cerr << "Error: " << context->errstr << std::endl;
    else
        freeReplyObject(reply);
}

std::string Logger::getCurrentTime()
{
    // To be replaced by the CelteTick time in the future
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char buffer[80];

    strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);

    return std::string(buffer);
}

std::string Logger::logLevelToString(LogLevel level)
{
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
