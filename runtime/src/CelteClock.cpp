#include "CelteClock.hpp"
#include "CelteRuntime.hpp"
#include "topics.hpp"

namespace celte {
namespace runtime {

struct ClockUpdate : public net::CelteRequest<ClockUpdate> {
  int tick;

  void to_json(nlohmann::json &j) const { j = nlohmann::json{{"tick", tick}}; }

  void from_json(const nlohmann::json &j) { j.at("tick").get_to(tick); }
};

void Clock::Init() {
  // subscribing to the global clock tick topic
  // KPOOL.Subscribe({
  //     .topics{celte::tp::GLOBAL_CLOCK},
  //     .autoCreateTopic = false,
  //     .callbacks{[this](auto r) { __updateCurrentTick(r); }},
  // });

  _createReaderStream<ClockUpdate>({
      .thisPeerUuid = RUNTIME.GetUUID(),
      .topics = {celte::tp::GLOBAL_CLOCK},
      .subscriptionName = "",
      .exclusive = false,
      .messageHandlerSync =
          [this](const pulsar::Consumer, ClockUpdate req) {
            __updateCurrentTick(req.tick);
          },
  });
}

void Clock::__updateCurrentTick(int tick) { _tick = tick; }

void Clock::ScheduleAt(int tick, std::function<void()> task) {
  if (tick < _tick) {
    task();
    return;
  }
  _tasks.push(std::make_pair(tick, task));
}

void Clock::CatchUp() {
  while (not _tasks.empty() and _tasks.top().first <= _tick) {
    _tasks.top().second();
    _tasks.pop();
  }
}

} // namespace runtime
} // namespace celte