#include "ReaderStream.hpp"

using namespace celte::net;

PendingRefCount::PendingRefCount(std::atomic_int &counter) : _counter(counter) {
  ++_counter;
}

PendingRefCount::~PendingRefCount() { --_counter; }
