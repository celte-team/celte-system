#pragma once
#include "kafka/Properties.h"
#include "queue.hpp"
#include <atomic>
#include <boost/thread.hpp>
#include <functional>
#include <kafka/AdminClient.h>
#include <kafka/KafkaConsumer.h>
#include <kafka/KafkaProducer.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace celte {
namespace nl {

class ConsumerWorker {
public:
  ConsumerWorker(ubn::queue<kafka::clients::consumer::ConsumerRecord> &records);
  ~ConsumerWorker();

  void AddConsumer(
      std::shared_ptr<kafka::clients::consumer::KafkaConsumer> consumer);

  inline int GetNumConsumers() {
    boost::lock_guard<boost::mutex> lock(*_consumerMutex);
    return _consumersAutoPoll.size();
  }

private:
  void __consumerJob();
  std::vector<std::shared_ptr<kafka::clients::consumer::KafkaConsumer>>
      _consumersAutoPoll;
  std::shared_ptr<boost::mutex> _consumerMutex;
  std::thread _consumerThread;
  std::atomic_bool _running;
  ubn::queue<kafka::clients::consumer::ConsumerRecord> &_recordsRef;
};

} // namespace nl
} // namespace celte