#pragma once
#include "boost/asio.hpp"
#include "boost/thread.hpp"
#include "queue.hpp"
#include <atomic>

namespace celte {
namespace nl {

class KConsumerService {
public:
  KConsumerService(
      const kafka::Properties &consumerProps, int numThreads,
      ubn::queue<kafka::clients::consumer::ConsumerRecord> &records);
  ~KConsumerService();

  void ExecSubscriptions(std::set<std::string> topics,
                         std::vector<std::function<void()>> thens);

  void ExecThens();

  inline void Stop() { _running = false; }

  void Start();

private:
  void __execSubscriptionsTask(std::set<std::string> topics,
                               std::vector<std::function<void()>> thens);
  void __pollOnceTask(
      std::shared_ptr<kafka::clients::consumer::KafkaConsumer> consumer);

  kafka::Properties _props;
  int _maxThenAtOnce = 100;
  std::atomic_bool _running = true;
  ubn::queue<std::function<void()>> _thens;
  boost::asio::io_service _ioService;
  boost::asio::io_service::work _work;
  boost::thread_group _threadGroup;
  ubn::queue<kafka::clients::consumer::ConsumerRecord> &_records;
  int _numThreads;
};
} // namespace nl
} // namespace celte