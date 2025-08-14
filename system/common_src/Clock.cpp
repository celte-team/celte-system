#include "Clock.hpp"
#include "Topics.hpp"
#include "systems_structs.pb.h"

using namespace celte;

Clock &Clock::Instance() {
  static Clock instance;
  return instance;
}

void Clock::Start() {
  _createReaderStream<req::ClockTick>(
      net::ReaderStream::Options<req::ClockTick>{
          .thisPeerUuid = RUNTIME.GetUUID(),
          .topics = {tp::global_clock()},
          .subscriptionName = RUNTIME.GetUUID() + "." + tp::global_clock(),
          .exclusive = false,
          .messageHandler = [this](const pulsar::Consumer &,
                                   req::ClockTick tick) {
            __updateCurrentTime(tick);
          }});
}

void Clock::Stop() { __cleanup(); }

Clock::timepoint Clock::GetUnifiedTime() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _lastTickValue +
         (std::chrono::system_clock::now() - _lastTickLocalTime);
}

void Clock::__updateCurrentTime(const req::ClockTick &tick) {
  std::lock_guard<std::mutex> lock(_mutex);
  _lastTickValue = std::chrono::time_point<std::chrono::system_clock>(
      std::chrono::milliseconds(tick.unified_time_ms()));
  _lastTickLocalTime = std::chrono::system_clock::now();
}

void Clock::ScheduleAt(const timepoint &unified_timepoint,
                       std::function<void()> task) {
  RUNTIME.ScheduleAsyncTask([this, unified_timepoint, task]() {
    auto now = GetUnifiedTime();
    if (now >= unified_timepoint) {
      task();
      return;
    }
    auto diff = unified_timepoint - now;
    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
    std::this_thread::sleep_for(diff_ms);
    task();
  });
}

std::string Clock::ToISOString(const timepoint &tp) {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm = *std::gmtime(&t);
  std::stringstream ss;
  ss << std::put_time(&tm, "%FT%T");
  return ss.str();
}

Clock::timepoint Clock::FromISOString(const std::string &str) {
  std::tm tm = {};
  std::stringstream ss(str);
  ss >> std::get_time(&tm, "%FT%T");
  std::time_t t = std::mktime(&tm);
  return std::chrono::system_clock::from_time_t(t);
}