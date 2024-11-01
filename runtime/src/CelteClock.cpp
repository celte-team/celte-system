#include "CelteClock.hpp"
#include "CelteRuntime.hpp"
#include "topics.hpp"

namespace celte {
namespace runtime {

void Clock::Init() {
  // subscribing to the global clock tick topic
  KPOOL.Subscribe({
      .topics{celte::tp::GLOBAL_CLOCK},
      .autoCreateTopic = false,
      .extraProps = {{"auto.offset.reset", "earliest"}},
      .autoPoll = true,
      .callbacks{[this](auto r) { __updateCurrentTick(r); }},
  });
}

void Clock::__updateCurrentTick(
    const kafka::clients::consumer::ConsumerRecord &r) {
  const char *data = reinterpret_cast<const char *>(r.value().data());
  std::string tickStr(data, r.value().size());
  _tick = std::stoi(tickStr);
}

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