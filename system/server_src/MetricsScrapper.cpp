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
  const std::string host =
      RUNTIME.GetConfig().Get("PUSHGATEWAY_HOST").value_or("localhost");
  const std::string port =
      RUNTIME.GetConfig().Get("PUSHGATEWAY_PORT").value_or("9091");
  const std::string job = RUNTIME.GetUUID();
  _url = "http://" + host + ":" + port + "/metrics/job/" + job;

  RegisterMetric<int>("cpu_load", [this]() { return __getCPULoad(); });
  _uploadThreadRunning = true;
  _uploadThread = std::thread(&MetricsScrapper::__uploadWorker, this);
  try {
    _metricsUploadInterval =
        std::stoi(RUNTIME.GetConfig().Get("METRICS_UPLOAD_INTERVAL").value());
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
    LOGERROR("Failed to upload metrics to pushgateway: " +
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
  static ULARGE_INTEGER lastSysKernel, lastSysUser, lastProcKernel,
      lastProcUser;
  FILETIME sysIdle, sysKernel, sysUser;
  FILETIME procCreation, procExit, procKernel, procUser;

  HANDLE hProcess = GetCurrentProcess();
  if (!GetSystemTimes(&sysIdle, &sysKernel, &sysUser) ||
      !GetProcessTimes(hProcess, &procCreation, &procExit, &procKernel,
                       &procUser)) {
    throw std::runtime_error("Failed to get times");
  }

  ULARGE_INTEGER sysKernelTime, sysUserTime, procKernelTime, procUserTime;
  memcpy(&sysKernelTime, &sysKernel, sizeof(FILETIME));
  memcpy(&sysUserTime, &sysUser, sizeof(FILETIME));
  memcpy(&procKernelTime, &procKernel, sizeof(FILETIME));
  memcpy(&procUserTime, &procUser, sizeof(FILETIME));

  if (lastSysKernel.QuadPart == 0) {
    lastSysKernel = sysKernelTime;
    lastSysUser = sysUserTime;
    lastProcKernel = procKernelTime;
    lastProcUser = procUserTime;
    return 0;
  }

  ULONGLONG sysTotal = (sysKernelTime.QuadPart - lastSysKernel.QuadPart) +
                       (sysUserTime.QuadPart - lastSysUser.QuadPart);
  ULONGLONG procTotal = (procKernelTime.QuadPart - lastProcKernel.QuadPart) +
                        (procUserTime.QuadPart - lastProcUser.QuadPart);

  lastSysKernel = sysKernelTime;
  lastSysUser = sysUserTime;
  lastProcKernel = procKernelTime;
  lastProcUser = procUserTime;

  if (sysTotal == 0)
    return 0;
  return static_cast<int>(100.0 * double(procTotal) / double(sysTotal));

#elif __linux__
  static long prevProcTime = 0, prevTotalTime = 0;
  long utime, stime;
  {
    std::ifstream statFile("/proc/self/stat");
    std::string ignore;
    for (int i = 0; i < 13; ++i)
      statFile >> ignore;
    statFile >> utime >> stime;
  }
  long procTime = utime + stime;

  long user, nice, system, idle, iowait, irq, softirq, steal;
  {
    std::ifstream file("/proc/stat");
    std::string line;
    std::getline(file, line);
    std::istringstream ss(line);
    std::string cpu;
    ss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >>
        steal;
  }
  long totalTime = user + nice + system + idle + iowait + irq + softirq + steal;

  long deltaProc = procTime - prevProcTime;
  long deltaTotal = totalTime - prevTotalTime;

  prevProcTime = procTime;
  prevTotalTime = totalTime;

  if (deltaTotal == 0)
    return 0;

  int numCpus = sysconf(_SC_NPROCESSORS_ONLN);
  return static_cast<int>(100.0 * (double(deltaProc) / double(deltaTotal)) *
                          numCpus);

#elif __APPLE__
  static uint64_t prevProcTime = 0, prevTotalTime = 0;

  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  uint64_t procTime = usage.ru_utime.tv_sec * 1e6 + usage.ru_utime.tv_usec +
                      usage.ru_stime.tv_sec * 1e6 + usage.ru_stime.tv_usec;

  host_cpu_load_info_data_t cpuinfo;
  mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
  if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                      (host_info_t)&cpuinfo, &count) != KERN_SUCCESS) {
    throw std::runtime_error("Failed to get CPU load info");
  }

  uint64_t totalTime =
      cpuinfo.cpu_ticks[CPU_STATE_USER] + cpuinfo.cpu_ticks[CPU_STATE_SYSTEM] +
      cpuinfo.cpu_ticks[CPU_STATE_IDLE] + cpuinfo.cpu_ticks[CPU_STATE_NICE];

  uint64_t deltaProc = procTime - prevProcTime;
  uint64_t deltaTotal = totalTime - prevTotalTime;

  prevProcTime = procTime;
  prevTotalTime = totalTime;

  if (deltaTotal == 0)
    return 0;

  int numCpus;
  size_t size = sizeof(numCpus);
  sysctlbyname("hw.ncpu", &numCpus, &size, NULL, 0);
  return static_cast<int>(
      100.0 * (double(deltaProc) / (double(deltaTotal) * 1e6)) * numCpus);
#else
  throw std::runtime_error("Unsupported platform");
#endif
}