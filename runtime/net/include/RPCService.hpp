#pragma once
#include "CelteNet.hpp"
#include "CelteRequest.hpp"
#include "CelteService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"
#include "WriterStreamPool.hpp"
#include "pulsar/Consumer.h"
#include "pulsar/Producer.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <future>
#include <optional>
#include <unordered_map>

namespace celte {
namespace net {

template <typename Ret> struct Awaitable {
  Awaitable(std::shared_ptr<std::future<Ret>> future,
            boost::asio::io_service &io)
      : _future(future), _io(io) {}

  void Then(std::function<void(Ret)> f) {
    _io.post([this, f]() {
      Ret ret = _future->get();
      CelteNet::Instance().PushThen([f = std::move(f), ret]() { f(ret); });
    });
  }

private:
  // std::future<Ret> _future;
  std::shared_ptr<std::future<Ret>> _future;
  boost::asio::io_service &_io;
};

struct RPRequest : public CelteRequest<RPRequest> {
  std::string name;       // the name of the rpc to invoke
  std::string respondsTo; // left empty for a call, set to the id of the rpc for
                          // a response of a previously called rpc
  std::string responseTopic; // where to send the response
  std::string rpcId;         // unique id for this rpc
  nlohmann::json args;       // arguments to the rpc

  void to_json(nlohmann::json &j) const {
    j = nlohmann::json{{"name", name},
                       {"respondsTo", respondsTo},
                       {"responseTopic", responseTopic},
                       {"rpcId", rpcId},
                       {"args", args}};
  }

  void from_json(const nlohmann::json &j) {
    j.at("name").get_to(name);
    j.at("respondsTo").get_to(respondsTo);
    j.at("responseTopic").get_to(responseTopic);
    j.at("rpcId").get_to(rpcId);
    j.at("args").get_to(args);
  }
};

/*
# Example usage:

Peer1:
```cpp
RPCService l;
l.Register("add", [](int a, int b) { return a + b; });
l.Register("foo", []() { return "bar"; });
```

Peer2:
```cpp
l.Call("add", 1, 2); // returns 3, blocking
l.CallAsync("foo").Then([](const std::string &result) { // then is pushed to the
queue of callbacks to execute std::cout << result << std::endl;
});
```

*/
class RPCService : public CelteService {
public:
  struct Options {
    const std::string &thisPeerUuid;
    std::string listenOn;
    int nThreads = 1;
  };

  RPCService(const Options &options);
  template <typename Ret, typename... Args>
  void Register(const std::string &name, std::function<Ret(Args...)> f) {

    _rpcs[name] = [f](const nlohmann::json &j) {
      std::tuple<Args...> args;
      j.get_to(args);
      return std::apply(f, args);
    };
  }

  template <typename Ret, typename... Args>
  Ret Call(const std::string &topic, const std::string &name, Args... args) {
    RPRequest req{
        .name = name,
        .respondsTo = "",
        .responseTopic = _options.listenOn,
        .rpcId = boost::uuids::to_string(boost::uuids::random_generator()()),
        .args = std::make_tuple(args...)};

    std::shared_ptr<std::promise<std::string>> promise =
        std::make_shared<std::promise<std::string>>();
    rpcPromises[req.rpcId] = promise;
    std::cout << "calling writer pool write" << std::endl;
    _writerStreamPool.Write(topic, req);

    std::string result = promise->get_future().get(); // json of response.args
    return nlohmann::json::parse(result).get<Ret>();
  }

  template <typename Ret, typename... Args>
  Awaitable<Ret> CallAsync(const std::string &topic, const std::string &name,
                           Args... args) {
    RPRequest req{
        .name = name,
        .respondsTo = "",
        .responseTopic = _options.listenOn,
        .rpcId = boost::uuids::to_string(boost::uuids::random_generator()()),
        .args = std::make_tuple(args...)};
    std::shared_ptr<std::promise<std::string>> promise =
        std::make_shared<std::promise<std::string>>();
    rpcPromises[req.rpcId] = promise;
    _writerStreamPool.Write(topic, req);

    // std::future<Ret> future = std::launch::async([promise]() {
    // std::future<Ret> future = std::async(std::launch::async, [promise]() {
    //   std::string result = promise->get_future().get();
    //   return nlohmann::json::parse(result).get<Ret>();
    // });

    std::shared_ptr<std::future<Ret>> future =
        std::make_shared<std::future<Ret>>(
            std::async(std::launch::async, [promise]() {
              std::string result = promise->get_future().get();
              return nlohmann::json::parse(result).get<Ret>();
            }));
    return Awaitable<Ret>(future, _io);
  }

private:
  void __initReaderStream(const std::string &topic);

  void __handleRemoteCall(const RPRequest &req);
  void __handleResponse(const RPRequest &req);

  boost::asio::io_service _io;
  boost::asio::io_service::work _work;
  boost::thread_group _threads;
  std::unordered_map<std::string,
                     std::function<nlohmann::json(const nlohmann::json &)>>
      _rpcs;
  Options _options;
  WriterStreamPool _writerStreamPool;
  std::unordered_map<std::string, std::shared_ptr<std::promise<std::string>>>
      rpcPromises;
};
} // namespace net
} // namespace celte