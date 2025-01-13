#pragma once
#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"
#include "WriterStreamPool.hpp"
#include "nlohmann/json.hpp"
#include "protos/systems_structs.pb.h"
#include "pulsar/Consumer.h"
#include "pulsar/Producer.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <unordered_map>
using namespace std::chrono_literals;

namespace celte {
namespace net {

template <typename Ret> struct Awaitable {
  Awaitable(std::shared_ptr<std::future<Ret>> future) : _future(future) {}

  void Then(std::function<void(Ret)> f,
            std::chrono::milliseconds timeout = 300ms) {
    std::shared_ptr<std::future<Ret>> future = this->_future;
    RUNTIME.ScheduleAsyncTask([this, f, future, timeout]() {
      if (future->wait_for(timeout) != std::future_status::ready) {
        std::cerr << "Timeout waiting for future" << std::endl;
        return;
      }
      Ret ret = future->get();
      f(ret);
    });
  }

private:
  std::shared_ptr<std::future<Ret>> _future;
  boost::asio::io_service &_io;
};

class RPCService : public CelteService {
public:
  static std::unordered_map<std::string,
                            std::shared_ptr<std::promise<std::string>>>
      rpcPromises;
  static std::mutex rpcPromisesMutex;

  struct Options {
    const std::string &thisPeerUuid;
    std::vector<std::string> listenOn;
    std::string reponseTopic = "";
    std::string serviceName = "";
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
    static_assert(std::is_base_of<google::protobuf::Message, Ret>::value,
                  "Ret must be a protobuf message.");
    req::RPRequest req;
    req.set_name(name);
    req.set_responds_to("");
    req.set_response_topic(_options.reponseTopic);
    req.set_rpc_id(boost::uuids::to_string(boost::uuids::random_generator()()));
    req.set_args(nlohmann::json(std::make_tuple(args...)).dump());

    std::shared_ptr<std::promise<std::string>> promise =
        std::make_shared<std::promise<std::string>>();
    {
      std::lock_guard<std::mutex> lock(rpcPromisesMutex);
      rpcPromises[req.rpc_id()] = promise;
    }
    _writerStreamPool.Write(topic, req);

    std::string result = promise->get_future().get(); // json of response.args
    // return nlohmann::json::parse(result).get<Ret>();
    // return google::protobuf::util::JsonStringToMessage<Ret>(result);
    Ret ret;
    if (!google::protobuf::util::JsonStringToMessage(result, &ret).ok()) {
      std::cerr << "Error parsing json to message: " << result << std::endl;
    }
    return ret;
  }

  template <typename... Args>
  void CallVoid(const std::string &topic, const std::string &name,
                Args... args) {
    req::RPRequest req;
    req.set_name(name);
    req.set_responds_to("");
    req.set_response_topic("");
    req.set_rpc_id(boost::uuids::to_string(boost::uuids::random_generator()()));
    req.set_args(nlohmann::json(std::make_tuple(args...)).dump());

    _writerStreamPool.Write(topic, req, [topic, name](pulsar::Result r) {
      if (r != pulsar::ResultOk) {
        std::cerr << "Error calling rpc on topic " << topic << " with name "
                  << name << std::endl;
      }
    });
  }

  template <typename Ret, typename... Args>
  Awaitable<Ret> CallAsync(const std::string &topic, const std::string &name,
                           Args... args) {
    req::RPRequest req;
    req.set_name(name);
    req.set_responds_to("");
    req.set_response_topic(_options.reponseTopic);
    req.set_rpc_id(boost::uuids::to_string(boost::uuids::random_generator()()));
    req.set_args(nlohmann::json(std::make_tuple(args...)).dump());

    std::shared_ptr<std::promise<std::string>> promise =
        std::make_shared<std::promise<std::string>>();
    {
      std::lock_guard<std::mutex> lock(rpcPromisesMutex);
      rpcPromises[req.rpc_id()] = promise;
    }

    auto future = std::make_shared<std::future<Ret>>(
        std::async(std::launch::async, [promise]() {
          std::string result = promise->get_future().get();
          Ret r;
          if (!google::protobuf::util::JsonStringToMessage(result, &r).ok()) {
            std::cerr << "Error parsing json to message: " << result
                      << std::endl;
          }
          return r;
        }));

    _writerStreamPool.Write(topic, req, [topic, name](pulsar::Result r) {
      if (r != pulsar::ResultOk) {
        std::cerr << "Error calling rpc on topic " << topic << " with name "
                  << name << std::endl;
      } else {
        std::cerr << "rpc call on topic " << topic << " with name " << name
                  << " succeeded" << std::endl;
      }
    });
    return Awaitable<Ret>(future);
  }

  inline bool Ready() {
    return std::all_of(_readerStreams.begin(), _readerStreams.end(),
                       [](auto &s) { return s->Ready(); });
  }

private:
  void __initReaderStream(const std::vector<std::string> &topic);

  void __handleRemoteCall(const req::RPRequest &req);
  void __handleResponse(const req::RPRequest &req);

  std::unordered_map<std::string,
                     std::function<nlohmann::json(const nlohmann::json &)>>
      _rpcs;
  Options _options;
  WriterStreamPool _writerStreamPool;
};
} // namespace net
} // namespace celte