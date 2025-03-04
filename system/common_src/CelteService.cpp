#include "CelteService.hpp"

using namespace celte::net;

void CelteService::__cleanup() {
  for (auto &rs : _readerStreams) {
    rs->Close();
  }
  // waiting a few ms so that all reader streams are closed properly and the
  // last message handler is done running
  for (auto &rs : _readerStreams) {
    rs->BlockUntilNoPending();
  }
  _readerStreams.clear();
  _writerStreams.clear();
}
