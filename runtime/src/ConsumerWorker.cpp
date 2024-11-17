#include "ConsumerWorker.hpp"

namespace celte {
namespace nl {

ConsumerWorker::ConsumerWorker(
    ubn::queue<kafka::clients::consumer::ConsumerRecord> &records)
    : _running(true), _consumerMutex(new boost::mutex()), _recordsRef(records) {
  _consumerThread = std::thread(&ConsumerWorker::__consumerJob, this);
}

ConsumerWorker::~ConsumerWorker() {
  _running = false;
  _consumerThread.join();
}

void ConsumerWorker::AddConsumer(
    std::shared_ptr<kafka::clients::consumer::KafkaConsumer> consumer) {
  boost::lock_guard<boost::mutex> lock(*_consumerMutex);
  _consumersAutoPoll.push_back(consumer);
}

void ConsumerWorker::__consumerJob() {
  while (_running) {
    {
      boost::lock_guard<boost::mutex> lock(*_consumerMutex);
      for (auto &consumer : _consumersAutoPoll) {
        auto records = consumer->poll(std::chrono::milliseconds(50));
        for (auto &record : records) {
          _recordsRef.push(record);
        }
      }
    }
  }
}

} // namespace nl
} // namespace celte
