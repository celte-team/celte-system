#include "WriterStreamPool.hpp"

namespace celte {
namespace net {
WriterStreamPool::WriterStreamPool(const Options &options,
                                   boost::asio::io_service &io)
    : _options(options), _running(true), _io(io) {
  _cleanupThread = std::thread([this]() {
    while (_running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::lock_guard<std::mutex> lock(_mutex);
      auto now = std::chrono::system_clock::now();
      for (auto it = _streams.begin(); it != _streams.end();) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.lastUsed)
                .count() > _options.idleTimeout.count()) {
          _streams.erase(it++);
        } else {
          ++it;
        }
      }
    }
  });
}

WriterStreamPool::~WriterStreamPool() {
  _running = false;
  std::cout << "joining " << std::endl;
  _cleanupThread.join();
  std::cout << "joined" << std::endl;
}

} // namespace net
} // namespace celte