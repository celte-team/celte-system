#include "Logger.hpp"

namespace celte {
namespace logs {

LogBuffer::LogBuffer(LogCallback callback) : callback(callback) {}

int LogBuffer::sync() {
  if (callback) {
    callback(str());
  }
  str(""); // Clear the buffer
  return 0;
}

Logger &Logger::getInstance() {
  static Logger instance;
  return instance;
}

Logger::Logger()
    : infoBuffer(nullptr), errorBuffer(nullptr), infoStream(&infoBuffer),
      errorStream(&errorBuffer) {}

void Logger::setPrinterInfo(LogCallback callback) {
  printerInfo = callback;
  infoBuffer = LogBuffer(printerInfo);
  infoStream.rdbuf(&infoBuffer);
}

void Logger::setPrinterError(LogCallback callback) {
  printerError = callback;
  errorBuffer = LogBuffer(printerError);
  errorStream.rdbuf(&errorBuffer);
}

std::ostream &Logger::info() { return infoStream; }

std::ostream &Logger::err() { return errorStream; }

} // namespace logs
} // namespace celte