#include "Config.hpp"
#include "Logger.hpp"
#include "MetricsScrapper.hpp"
#include "Runtime.hpp"
#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#include <pdh.h>
#include <windows.h>
#pragma comment(lib, "pdh.lib")
#elif __linux__
#include <fstream>
#include <unistd.h>
#elif __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

using namespace celte;

MetricsScrapper &MetricsScrapper::GetInstance() {
  static MetricsScrapper instance;
  return instance;
}

MetricsScrapper::MetricsScrapper() {
  curl_global_init(CURL_GLOBAL_ALL);
  _curl = curl_easy_init();
}

MetricsScrapper::~MetricsScrapper() {
  if (_curl)
    curl_easy_cleanup(_curl);
  curl_global_cleanup();

  _uploadThreadRunning = false;
  if (_uploadThread.joinable())
    _uploadThread.join();
}

void MetricsScrapper::Start() {
  const std::string host = RUNTIME.GetConfig().Get("pushgateway_host").value();
  const std::string port = RUNTIME.GetConfig().Get("pushgateway_port").value();
  const std::string job = RUNTIME.GetUUID();
  _url = "http://" + host + ":" + port + "/metrics/job/" + job;

  RegisterMetric<int>("cpu_load", [this]() { return __getCPULoad(); });
  _uploadThreadRunning = true;
  _uploadThread = std::thread(&MetricsScrapper::__uploadWorker, this);

  try {
    _metricsUploadInterval =
        std::stoi(RUNTIME.GetConfig().Get("metrics_upload_interval").value());
  } catch (const std::exception &e) {
    LOGERROR("Failed to get metrics upload interval: " + std::string(e.what()));
  }
}

void MetricsScrapper::PushMetrics() {
  if (!_curl)
    return;

  std::string metrics_data;
  {
    std::lock_guard<std::mutex> lock(_metricsMutex);
    for (const auto &metric : _metrics) {
      try {
        metrics_data += metric.first + " " + metric.second() + "\n";
      } catch (const std::exception &e) {
        LOGERROR("Failed to get metric " + metric.first + ": " + e.what());
      }
    }
  }
  curl_easy_setopt(_curl, CURLOPT_URL, _url.c_str());
  curl_easy_setopt(_curl, CURLOPT_POSTFIELDS, metrics_data.c_str());

  CURLcode res = curl_easy_perform(_curl);
  if (res != CURLE_OK) {
    THROW_ERROR(MetricsUploadException,
                "Failed to upload metrics to pushgateway: " +
                    std::string(curl_easy_strerror(res)));
  }
}

void MetricsScrapper::__uploadWorker() {
  while (true) {
    PushMetrics();
    std::this_thread::sleep_for(std::chrono::seconds(_metricsUploadInterval));
  }
}

int MetricsScrapper::__getCPULoad() {
#ifdef _WIN32
  static PDH_HQUERY cpuQuery;
  static PDH_HCOUNTER cpuTotal;
  static bool initialized = false;

  if (!initialized) {
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL,
                  &cpuTotal);
    PdhCollectQueryData(cpuQuery);
    initialized = true;
  }

  PdhCollectQueryData(cpuQuery);
  PDH_FMT_COUNTERVALUE counterVal;
  PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
  return static_cast<int>(counterVal.doubleValue);

#elif __linux__
  static long prevIdleTime = 0;
  static long prevTotalTime = 0;

  std::ifstream file("/proc/stat");
  std::string line;
  std::getline(file, line);
  file.close();

  std::istringstream ss(line);
  std::string cpu;
  long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
  ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >>
      steal >> guest >> guest_nice;

  long idleTime = idle + iowait;
  long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;

  long deltaIdle = idleTime - prevIdleTime;
  long deltaTotal = totalTime - prevTotalTime;

  prevIdleTime = idleTime;
  prevTotalTime = totalTime;

  return static_cast<int>(
      100.0 * (1.0 - (deltaIdle / static_cast<double>(deltaTotal))));

#elif __APPLE__
  host_cpu_load_info_data_t cpuinfo;
  mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
  if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                      (host_info_t)&cpuinfo, &count) != KERN_SUCCESS) {
    throw std::runtime_error("Failed to get CPU load info");
  }

  static uint64_t prevIdleTime = 0;
  static uint64_t prevTotalTime = 0;

  uint64_t idleTime = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
  uint64_t totalTime = idleTime + cpuinfo.cpu_ticks[CPU_STATE_USER] +
                       cpuinfo.cpu_ticks[CPU_STATE_SYSTEM] +
                       cpuinfo.cpu_ticks[CPU_STATE_NICE];

  uint64_t deltaIdle = idleTime - prevIdleTime;
  uint64_t deltaTotal = totalTime - prevTotalTime;

  prevIdleTime = idleTime;
  prevTotalTime = totalTime;

  return static_cast<int>(
      100.0 * (1.0 - (deltaIdle / static_cast<double>(deltaTotal))));

#else
  throw std::runtime_error("Unsupported platform");
#endif
}