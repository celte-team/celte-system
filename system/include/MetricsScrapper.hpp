#pragma once
#include "CelteError.hpp"
#include <array>
#include <atomic>
#include <cstdio>
#include <curl/curl.h>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#ifndef CELTE_SERVER_MODE_ENABLED
#error "Metrics are only available in server mode"
#endif

namespace celte {
class MetricsUploadException : public CelteError {
public:
  MetricsUploadException(const std::string &msg, RedisDb &log,
                         std::string file = __FILE__, int line = __LINE__)
      : CelteError(msg, log, file, line,
                   [msg](std::string s) { std::cerr << msg << std::endl; }) {}
};

/// @brief This class is responsible for scrapping useful metrics
/// from the system, and exposing them to the user so that the user's
/// custom code can monitor them.
/// Metrics are pushed to a pushgateway so that prometheus can scrape them
/// dynamically.
class MetricsScrapper {
public:
  static MetricsScrapper &GetInstance();
  MetricsScrapper();
  ~MetricsScrapper();
  MetricsScrapper(const MetricsScrapper &) = delete;
  MetricsScrapper &operator=(const MetricsScrapper &) = delete;

  /// @brief Registers a metric to be scrapped using the provided getter.
  /// This metric will then be published to prometheus.
  /// @tparam T
  /// @param name
  /// @param getter
  template <typename T>
  void RegisterMetric(const std::string &name, std::function<T()> getter) {
    std::lock_guard<std::mutex> lock(_metricsMutex);
    _metrics[name] = [getter]() {
      std::array<char, 64> buffer;
      std::stringstream ss;
      ss << getter();
      return ss.str();
    };
  }

  /// @brief Pushes all the registered metrics to the pushgateway.
  void PushMetrics();

  /// Starts the metrics upload thread.
  void Start();

private:
  void __uploadWorker();

  // default metrics

  /// @brief Get the percentage of the CPU used by this process.
  /// @return
  int __getCPULoad();

  std::unordered_map<std::string, std::function<std::string()>> _metrics;
  int _metricsUploadInterval = 5; // seconds
  CURL *_curl;
  std::string _url;
  std::mutex _metricsMutex;
  std::thread _uploadThread;
  std::atomic_bool _uploadThreadRunning;
};

/// @brief Metrics Scrapper singleton.
#define METRICS MetricsScrapper::GetInstance()
} // namespace celte