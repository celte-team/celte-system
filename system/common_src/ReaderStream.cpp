#include "ReaderStream.hpp"
#include "Runtime.hpp"
#include <boost/bind/bind.hpp>

void handleMessageDelegate(
    pulsar::Message msg, std::shared_ptr<pulsar::Consumer> consumer,
    std::shared_ptr<std::atomic_bool> closed,
    std::function<void(const std::string &)> messageHandler,
    std::shared_ptr<std::atomic_int> pendingMessages) {
  celte::net::PendingRefCount prc(*pendingMessages); // RAII counter for pending
  if (*closed) {
    std::cout << "Consumer closed, stopping message handling." << std::endl;
    return; // Exit if closed
  }
  if (msg.getLength() > 0) {
    std::string data(static_cast<const char *>(msg.getData()), msg.getLength());
    consumer->acknowledge(msg);

    // {
    //   // debug
    //   if (data.find("unified_time_ms") == std::string::npos) {
    //     std::cout << "Received message: " << data << std::endl;
    //   }
    // }

    messageHandler(data);
  } else {
    std::cerr << "Received empty message, skipping." << std::endl;
  }
}

using namespace celte::net;

void ReaderStream::BlockUntilNoPending() {
  while (_pendingMessages > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
