#pragma once
#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"
#include "WriterStreamPool.hpp"
#include "nlohmann/json.hpp"
#include "pulsar/Consumer.h"
#include "pulsar/Producer.h"
#include "systems_structs.pb.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "CelteError.hpp"
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

class RPCTimeoutException : public CelteError {
public:
  RPCTimeoutException(const std::string &msg, Logger &log,
                      std::string file = __FILE__, int line = __LINE__)
      : CelteError(msg, log, file, line,
                   [](std::string s) { RUNTIME.Hooks().onRPCTimeout(s); }) {}
};

class RPCHandlingException : public CelteError {
public:
  RPCHandlingException(const std::string &msg, Logger &log,
                       std::string file = __FILE__, int line = __LINE__)
      : CelteError(msg, log, file, line, [](std::string s) {
          RUNTIME.Hooks().onRPCHandlingError(s);
        }) {}
};

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
      try {
        Ret ret = future->get();
        f(ret);
      } catch (std::exception &e) {
        std::cerr << "Error in async rpc future: " << e.what() << std::endl;
      }
    });
  }

private:
  std::shared_ptr<std::future<Ret>> _future;
};

class RPCService : public CelteService {
public:
  static std::unordered_map<std::string,
                            std::shared_ptr<std::promise<std::string>>>
      rpcPromises;
  static std::mutex rpcPromisesMutex;

  struct Options {
    std::string thisPeerUuid = "";
    std::vector<std::string> listenOn = {};
    std::string reponseTopic = "";
    std::string serviceName = "";

    Options &operator=(const Options &other) {
      if (this != &other) {
        const_cast<std::string &>(thisPeerUuid) = other.thisPeerUuid;
        listenOn = other.listenOn;
        reponseTopic = other.reponseTopic;
        serviceName = other.serviceName;
      }
      return *this;
    }
  };

  RPCService();

  RPCService(const Options &options);
  void Init(const Options &options);
  template <typename Ret, typename... Args>
  void Register(const std::string &name, std::function<Ret(Args...)> f) {

    _rpcs[name] = [f](const nlohmann::json &j) {
      std::tuple<Args...> args;
      j.get_to(args);
      return std::apply(f, args);
    };
  }

  /// @brief Equivalent to using 'Call' but 'Call' has a set timeout of 300ms.
  /// This function lets you set the timeout.
  template <typename Ret, typename... Args>
  Ret CallWithTimeout(const std::string &topic, const std::string &name,
                      std::chrono::milliseconds timeout, Args... args) {
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

    std::string result;
    auto fut = promise->get_future();
    if (fut.wait_for(timeout) == std::future_status::timeout) {
      throw RPCTimeoutException("Timeout waiting for rpc response", LOGGER);
    }
    result = fut.get();

    try {
      Ret ret = nlohmann::json::parse(result).get<Ret>();
      return ret;
    } catch (nlohmann::json::exception &e) {
      std::cerr << "Error parsing json: " << e.what() << std::endl;
      throw e; // todo custom error type
    }
  }

  template <typename Ret, typename... Args>
  Ret Call(const std::string &topic, const std::string &name, Args... args) {
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

    std::string result;
    auto fut = promise->get_future();
    if (fut.wait_for(300ms) == std::future_status::timeout) {
      throw RPCTimeoutException("Timeout waiting for rpc response", LOGGER);
    }
    result = fut.get();

    try {
      Ret ret = nlohmann::json::parse(result).get<Ret>();
      return ret;
    } catch (nlohmann::json::exception &e) {
      std::cerr << "Error parsing json: " << e.what() << std::endl;
      throw e; // todo custom error type
    }
  }

  template <typename Ret>
  Ret Call(const std::string &topic, const std::string &name) {
    req::RPRequest req;
    req.set_name(name);
    req.set_responds_to("");
    req.set_response_topic(_options.reponseTopic);
    req.set_rpc_id(boost::uuids::to_string(boost::uuids::random_generator()()));
    req.set_args("");

    std::shared_ptr<std::promise<std::string>> promise =
        std::make_shared<std::promise<std::string>>();
    {
      std::lock_guard<std::mutex> lock(rpcPromisesMutex);
      rpcPromises[req.rpc_id()] = promise;
    }
    _writerStreamPool.Write(topic, req);

    std::string result;
    auto fut = promise->get_future();
    if (fut.wait_for(300ms) == std::future_status::timeout) {
      throw RPCTimeoutException("Timeout waiting for rpc response", LOGGER);
    }
    result = fut.get();

    try {
      Ret ret = nlohmann::json::parse(result).get<Ret>();
      return ret;
    } catch (nlohmann::json::exception &e) {
      std::cerr << "Error parsing json: " << e.what() << std::endl;
      throw e; // todo custom error type
    }
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
          try {
            Ret ret = nlohmann::json::parse(result).get<Ret>();
            return ret;
          } catch (nlohmann::json::exception &e) {
            std::cerr << "Error parsing json: " << e.what() << std::endl;
            throw e; // todo custom error type
          }
        }));

    _writerStreamPool.Write(topic, req, [topic, name](pulsar::Result r) {
      if (r != pulsar::ResultOk) {
        std::cerr << "Error calling rpc on topic " << topic << " with name "
                  << name << std::endl;
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
