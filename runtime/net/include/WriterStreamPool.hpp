#pragma once
#include "CelteNet.hpp"
#include "CelteRequest.hpp"
#include "WriterStream.hpp"
#include "pulsar/Producer.h"

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <mutex>

namespace celte {
namespace net {
/**
 * @brief Apache pulsar does not support producing on multiple topics using a
 * single producer. This class implements a pool of producers that can be
 used
 * to produce on multiple topics. When a topic is requested but does not have
 a
 * producer, a new producer is created and added to the pool. When a producer
 is
 * not used for a certain amount of time, it is destroyed to save resources.
 */
class WriterStreamPool {
public:
  struct Options {
    std::chrono::milliseconds idleTimeout = std::chrono::milliseconds(1000);
  };

  struct WriterStreamPoolEntry {
    std::shared_ptr<WriterStream> producer;
    std::chrono::time_point<std::chrono::system_clock> lastUsed;
  };

  /**
   * @brief Construct a new Writer Stream Pool object.
   * The io service is used to run any asynchronous operations need by the pool.
   * The service must have been initialized before the pool is created and
   * must be running.
   * This instance of the pool wil not take ownership of the io service and must
   * be destroyed before the io service is destroyed.
   */
  WriterStreamPool(const Options &options, boost::asio::io_service &io);
  ~WriterStreamPool();

  /**
   * @brief Write a request to a topic.
   *
   * @tparam Req The request type.
   * @param topic The topic to write to.
   * @param req The request to write.
   */
  template <typename Req> void Write(const std::string &topic, const Req &req) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _streams.find(topic);
    if (it == _streams.end()) {
      // create the stream and write the request as soon as it is ready
      std::cout << "creating stream" << std::endl;
      std::shared_ptr<WriterStream> stream =
          std::make_shared<WriterStream>(WriterStream::Options{
              .topic = topic, .onReady = [req](WriterStream &s) {
                std::cout << "producer is ready" << std::endl;
                s.Write(req);
              }});
      std::cout << "opening stream" << std::endl;
      stream->Open<Req>();
      std::cout << "pushing stream to pool" << std::endl;

      _streams[topic] = WriterStreamPoolEntry{
          .producer = stream, .lastUsed = std::chrono::system_clock::now()};
      return;
    }
    std::cout << "stream found" << std::endl;

    // if the producer is not ready, wait until it is, else write the request
    // immediately
    auto stream = it->second;
    if (!stream.producer->Ready()) {
      std::cout << "post writing" << std::endl;
      _io.post([this, topic, req, stream]() {
        // wait until the producer is ready
        std::cout << "waiting for producer to be ready" << std::endl;
        while (!stream.producer->Ready())
          ;
        std::cout << "sending" << std::endl;
        stream.producer->Write(req);
      });
    } else {
      std::cout << "writing directly" << std::endl;
      stream.producer->Write(req);
    }

    stream.lastUsed = std::chrono::system_clock::now();
  }

private:
  Options _options;
  std::unordered_map<std::string, WriterStreamPoolEntry> _streams;
  std::mutex _mutex;
  boost::asio::io_service &_io;
  std::atomic_bool _running;
  std::thread _cleanupThread;
};
} // namespace net
} // namespace celte