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
** main
*/

#include "CelteError.hpp"
#include "Logger.hpp"
#include <iostream>
#include <unistd.h>

int main()
{
    try {
        std::string redis_key = "log_messages_" + std::to_string(getpid());

        Logger logger("127.0.0.1", 6379, redis_key, "test");
        logger.log(Logger::DEBUG, "This is a debug message");
        logger.log(Logger::WARNING, "This is a warning message");
        logger.log(Logger::ERROR, "This is an error message");

        THROW_ERROR(CelteError, "This is a test error", logger);
        // THROW_ERROR_CB(CelteError, "This is a test error", logger, [](const std::string& msg) {
        //     std::cerr << "Callback: " << msg << std::endl;
        // });
    } catch (const CelteError& e) {
        std::cerr << "Caught a RedisError: " << e.what() << std::endl;
    }
}
