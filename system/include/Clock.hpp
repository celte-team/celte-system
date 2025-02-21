#pragma once
#include "CelteService.hpp"
#include "systems_structs.pb.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#define CLOCK celte::Clock::Instance()

namespace celte {
class Clock : net::CelteService {
public:
  static Clock &Instance();
  using timepoint = std::chrono::time_point<std::chrono::system_clock>;

  void Start();
  void Stop();

  /// @brief Returns a time point representing the current unified time on the
  /// cluster. All peers have the same unified time.
  timepoint GetUnifiedTime();

  /// @brief Schedules a task to run at a specific time.
  /// @param unified_timepoint The time at which the task should run.
  /// @note The task will run in a separate thread. Use the _ms_later operator
  /// to easily create time points in the future.
  /// @example
  /// CLOCK.ScheduleAt(1000_ms_later, []() { std::cout << "Hello, world!" <<
  /// std::endl; });
  void ScheduleAt(const timepoint &unified_timepoint,
                  std::function<void()> task);

  /// @brief Converts a timepoint to an ISO 8601 formatted string.
  static std::string ToISOString(const timepoint &tp);

  /// @brief Parses an ISO 8601 formatted string to a timepoint.
  static timepoint FromISOString(const std::string &str);

private:
  void __updateCurrentTime(const req::ClockTick &tick);
  std::mutex _mutex;
  timepoint _lastTickValue = std::chrono::system_clock::now();
  timepoint _lastTickLocalTime = std::chrono::system_clock::now();
};

/// @brief Returns a time point representing a point val milliseconds in the
/// future (united cluster time)
inline celte::Clock::timepoint
operator""_ms_later(const unsigned long long int val) {
  return celte::Clock::Instance().GetUnifiedTime() +
         std::chrono::milliseconds(val);
}
} // namespace celte
