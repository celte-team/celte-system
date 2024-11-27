#include "KConsumerService.hpp"

namespace celte {
namespace nl {
KConsumerService::KConsumerService(
    const kafka::Properties &consumerProps, int numThreads,
    ubn::queue<kafka::clients::consumer::ConsumerRecord> &records)
    : _props(consumerProps), _work(_ioService), _records(records),
      _numThreads(numThreads) {}

KConsumerService::~KConsumerService() {
  _ioService.stop();
  _running = false;
  _threadGroup.join_all();
}

void KConsumerService::Start() {
  for (int i = 0; i < _numThreads; i++) {
    _threadGroup.create_thread([this]() { _ioService.run(); });
  }
  _running = true;
}

void KConsumerService::ExecSubscriptions(
    std::set<std::string> topics, std::vector<std::function<void()>> thens) {
  _ioService.post(
      [this, topics, thens]() { __execSubscriptionsTask(topics, thens); });
}

void KConsumerService::ExecThens() {
  int i = 0;
  while (!_thens.empty() or ++i > _maxThenAtOnce) {
    _thens.pop()();
  }
}

void KConsumerService::__execSubscriptionsTask(
    std::set<std::string> topics, std::vector<std::function<void()>> thens) {
  std::shared_ptr<kafka::clients::consumer::KafkaConsumer> consumer =
      std::make_shared<kafka::clients::consumer::KafkaConsumer>(_props);
  consumer->subscribe(topics);
  for (auto &then : thens) {
    if (then != nullptr)
      _thens.push(then);
  }
  _ioService.post([this, consumer]() { __pollOnceTask(consumer); });
}

void KConsumerService::__pollOnceTask(
    std::shared_ptr<kafka::clients::consumer::KafkaConsumer> consumer) {
  auto records = consumer->poll(std::chrono::milliseconds(100));
  for (auto &record : records) {
    _records.push(record);
  }
  if (_running)
    _ioService.post([this, consumer]() { __pollOnceTask(consumer); });
}

} // namespace nl
} // namespace celte