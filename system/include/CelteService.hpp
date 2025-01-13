#pragma once
#include "CelteNet.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"
#include "nlohmann/json.hpp"
#include "pulsar/Schema.h"
#include <optional>

namespace celte {
namespace net {

// This class is the base class for all network services. It provides basic
// functionality for connecting to the network and sending and receiving
// messages, and creating a stream of messages.
class CelteService {
public:
  inline std::optional<std::shared_ptr<WriterStream>>
  GetWriterStream(const std::string &topic) {
    if (_writerStreams.find(topic) == _writerStreams.end())
      return std::nullopt;
    return _writerStreams[topic];
  }

protected:
  inline void _destroyReaderStream(std::shared_ptr<ReaderStream> stream) {
    _readerStreams.erase(
        std::remove_if(_readerStreams.begin(), _readerStreams.end(),
                       [stream](auto &s) { return s == stream; }),
        _readerStreams.end());
  }

  inline void _destroyAllReaderStreams() { _readerStreams.clear(); }

  inline void _destroyWriterStream(const std::string &topic) {
    _writerStreams.erase(topic);
  }

  inline void _destroyAllWriterStreams() { _writerStreams.clear(); }

  template <typename Req>
  std::shared_ptr<ReaderStream>
  _createReaderStream(ReaderStream::Options<Req> options) {
    auto rs = std::make_shared<ReaderStream>();
    _readerStreams.push_back(rs);
    rs->Open<Req>(options);
    return rs;
  }

  template <typename Req>
  std::shared_ptr<WriterStream>
  _createWriterStream(const WriterStream::Options options) {
    auto ws = std::make_shared<WriterStream>(options);
    _writerStreams[options.topic] = ws;
    ws->Open<Req>();
    return ws;
  }

  std::unordered_map<std::string, std::shared_ptr<WriterStream>> _writerStreams;
  std::vector<std::shared_ptr<ReaderStream>> _readerStreams;
};
} // namespace net
} // namespace celte