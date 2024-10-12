#pragma once

#include <functional>
#include <iostream>
#include <sstream>

namespace celte {
namespace logs {

class LogBuffer : public std::stringbuf {
public:
  using LogCallback = std::function<void(const std::string &)>;

  LogBuffer(LogCallback callback);

  int sync() override;

private:
  LogCallback callback;
};

class Logger {
public:
  using LogCallback = std::function<void(const std::string &)>;

  static Logger &getInstance();

  void setPrinterInfo(LogCallback callback);
  void setPrinterError(LogCallback callback);

  std::ostream &info();
  std::ostream &err();

private:
  Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  LogBuffer infoBuffer;
  LogBuffer errorBuffer;
  std::ostream infoStream;
  std::ostream errorStream;

  LogCallback printerInfo;
  LogCallback printerError;
};

} // namespace logs
} // namespace celte