#include "ReaderStream.hpp"

using namespace celte::net;

void ReaderStream::BlockUntilNoPending() {
  while (_pendingMessages > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}