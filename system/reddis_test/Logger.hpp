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

#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <string>

class Logger {
public:
    enum LogLevel {
        DEBUG,
        WARNING,
        ERROR,
        FATAL
    };

    // uuid should be the RUNTIME.currentUUID
    // key can be the pid to split logs by process, OR common to all process to centrilize them
    Logger(const std::string& redis_host, int redis_port, const std::string& key, const std::string& uuid);
    ~Logger();

    //  log should be async, add a function to a pool to not slow the process
    void log(LogLevel level, const std::string& message);

private:
    std::string logLevelToString(LogLevel level);
    std::string getCurrentTime();

    std::shared_ptr<redisContext> context;
    std::mutex log_mutex;
    std::string redis_key;
    std::string uuid;
};
