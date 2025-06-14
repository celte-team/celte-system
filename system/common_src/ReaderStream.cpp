#include "ReaderStream.hpp"
#include "Runtime.hpp"
#include <boost/bind/bind.hpp>

using namespace celte::net;

void ReaderStream::BlockUntilNoPending() {
  while (_pendingMessages > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

static void poll(std::shared_ptr<pulsar::Consumer> consumer,
                 std::shared_ptr<std::atomic_bool> closed,
                 std::function<void(const std::string &)> messageHandler,
                 std::shared_ptr<std::atomic_int> pendingMessages) {
  // RUNTIME.ScheduleAsyncIOTask([consumer, closed, messageHandler,
  //                              pendingMessages]() {
  std::thread([consumer, closed, messageHandler, pendingMessages]() mutable {
    // if (!*closed) {
    while (!*closed) {
      pulsar::Message msg;
      PendingRefCount prc(*pendingMessages); // RAII counter for pending
      pulsar::Result result = consumer->receive(msg);

      if (result == pulsar::ResultOk) {

        std::string data(static_cast<const char *>(msg.getData()),
                         msg.getLength());
        consumer->acknowledge(msg);

        // {
        //   // debug
        //   if (data.find("unified_time_ms") == std::string::npos) {
        //     std::cout << "Received message: " << data << std::endl;
        //   }
        // }

        // poll(consumer, closed, messageHandler, pendingMessages);
        messageHandler(data);
      } else if (result == pulsar::ResultTimeout) {
        // poll(consumer, closed, messageHandler, pendingMessages);
        continue;
      } else {
        consumer->negativeAcknowledge(msg); // Negative acknowledge on error
        std::cerr << "Error receiving message: " << pulsar::strResult(result)
                  << std::endl;
        // poll(consumer, closed, messageHandler, pendingMessages);
        continue;
      }
    }
  }).detach(); // Detach the thread to run independently
}

void ReaderStream::__startPolling(std::shared_ptr<pulsar::Consumer> consumer) {
  poll(consumer, _closed, _messageHandler, _pendingMessages);
}
