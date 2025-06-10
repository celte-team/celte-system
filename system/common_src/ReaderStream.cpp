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
  consumer->receiveAsync([consumer, closed, messageHandler, pendingMessages](
                             pulsar::Result result, pulsar::Message msg) {
    PendingRefCount prc(*pendingMessages); // RAII counter for pending ops.

    if (*closed) {
      return; // If the stream is closed, do not process any more messages.
    }
    if (result == pulsar::ResultOk) {
      std::string data(static_cast<const char *>(msg.getData()),
                       msg.getLength());
      poll(consumer, closed, messageHandler,
           pendingMessages); // Schedule the next poll
      messageHandler(data);
      consumer->acknowledge(msg);
    } else {
      std::cerr << "Error receiving message: " << result << std::endl;
      poll(consumer, closed, messageHandler,
           pendingMessages); // Retry polling on error
    }
  });
}

void ReaderStream::__startPolling(std::shared_ptr<pulsar::Consumer> consumer) {
  poll(consumer, _closed, _messageHandler, _pendingMessages);
}
