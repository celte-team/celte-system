#pragma once
#include "CelteNet.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <future>
#include <memory>
namespace celte {
namespace net {

template <typename Ret> struct Awaitable {
  Awaitable(std::shared_ptr<std::future<Ret>> future,
            boost::asio::io_service &io)
      : _future(future), _io(io) {}

  void Then(std::function<void(Ret)> f) {
    std::shared_ptr<std::future<Ret>> future = this->_future;
    _io.post([this, f, future]() {
      Ret ret = future->get();
      CelteNet::Instance().PushThen([f = std::move(f), ret]() { f(ret); });
    });
  }

private:
  std::shared_ptr<std::future<Ret>> _future;
  boost::asio::io_service &_io;
};

} // namespace net
} // namespace celte