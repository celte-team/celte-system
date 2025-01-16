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
** CelteError
*/

#pragma once
#include "Logger.hpp"
#include <exception>
#include <functional>
#include <iostream>
#include <string>
// To change : Logger should be from a singleton instead of given by reference

#define THROW_ERROR(errorType, msg)                                            \
  do {                                                                         \
    errorType error(msg, LOGGER, __FILE__, __LINE__);                          \
    throw error;                                                               \
  } while (0)

#define THROW_ERROR_CB(errorType, msg, callback)                               \
  do {                                                                         \
    errorType error(msg, LOGGER, __FILE__, __LINE__, callback);                \
    throw error;                                                               \
  } while (0)

namespace celte {
class CelteError : public std::exception {
private:
  int line;
  std::string file;
  std::string _message;
  Logger &logger;
  std::function<void(const std::string &)> _callback;

public:
  CelteError(const std::string &msg, Logger &log, std::string file = __FILE__,
             int line = __LINE__,
             const std::function<void(const std::string &)> &callback = nullptr)
      : _message("At " + file + " - " + std::to_string(line) + " : " + msg),
        logger(log), _callback(callback) {
    try {
      logger.log(Logger::ERROR,
                 "At " + file + " - " + std::to_string(line) + " : " + msg);
      if (callback) {
        callback(_message);
      }
    } catch (const std::exception &e) {
      std::cerr << "Exception: " << e.what() << std::endl;
      std::cerr << "At Error: " << _message << std::endl;
    }
  }

  const char *what() const noexcept override { return _message.c_str(); }
};
} // namespace celte