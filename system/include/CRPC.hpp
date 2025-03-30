#pragma once
#include "Logger.hpp"
#include "systems_structs.pb.h"
#include <Runtime.hpp>
#include <algorithm>
#include <any>
#include <atomic>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <google/protobuf/util/json_util.h>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <pulsar/Client.h>
#include <string>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>
#include <typeindex>
#include <unordered_map>
#include <variant>

static const std::string PERSISTENT_DEFAULT = "persistent://public/default/";
static std::string static_uuid = "peer1";

namespace celte {
namespace detail {

/// @brief The LRUSet class is a thread-safe set that keeps track of the least
/// recently used keys. When the set reaches its capacity, the least recently
/// used key is removed.
/// @tparam KeyType
template <typename KeyType> class LRUSet {
public:
  LRUSet(size_t capacity = 100) : _capacity(capacity) {}

  /// @brief Adds a key to the set.
  /// @param key
  void add(const KeyType &key) {
    std::lock_guard<std::mutex> lock(_mutex);
    typename tbb::concurrent_hash_map<
        KeyType, typename std::list<KeyType>::iterator>::accessor accessor;
    if (_cacheMap.find(accessor, key)) {
      // the key already exists, move it to the front
      _cacheList.erase(accessor->second);
      _cacheList.push_front(key);
      accessor->second = _cacheList.begin();
    } else {
      // the key does not exist, add it
      if (_cacheList.size() >= _capacity) {
        // we remove the least recently used key
        KeyType lruKey = _cacheList.back();
        _cacheList.pop_back();
        _cacheMap.erase(lruKey);
      }
      _cacheList.push_front(key);
      _cacheMap.insert(accessor, key);
      accessor->second = _cacheList.begin();
    }
  }

  /// @brief Returns true if the set contains the key, false otherwise.
  /// @param key
  /// @return
  bool contains(const KeyType &key) const {
    typename tbb::concurrent_hash_map<
        KeyType, typename std::list<KeyType>::iterator>::const_accessor
        accessor;
    return _cacheMap.find(accessor, key);
  }

private:
  size_t _capacity;
  mutable std::mutex _mutex;
  std::list<KeyType> _cacheList;
  tbb::concurrent_hash_map<KeyType, typename std::list<KeyType>::iterator>
      _cacheMap;
};

/// @brief Wrapper task to keep trace of running tasks. Tasks that are already
/// running of have been ran are not ran again.
class UniqueTaskManager {
public:
  /// @brief Runs the task with the specified id. If the task is already running
  /// or has been ran, it is not ran again. The task is ran synchronously.
  /// @tparam Task
  /// @param task_id
  /// @param task
  template <typename Task> bool run(const std::string &task_id, Task &&task) {
    if (!_runningTasks.contains(task_id)) {
      _runningTasks.add(task_id);
      task();
      return true;
    }
    return false;
  }

private:
  LRUSet<std::string> _runningTasks;
};

/// @brief Pool of producers to write to apache pulsar topics.
/// The pool is thread safe.
/// Use the write method to write to a topic. A producer will be created for
/// the topic if it does not exist yet.
/// Unused producers are removed from the pool after a certain time.
class ApachePulsarRPCProducerPool {
public:
  struct ProducerBucket {
    std::shared_ptr<pulsar::Producer> producer;
    std::chrono::time_point<std::chrono::system_clock> lastUsed;
  };

  using producer_accessor =
      tbb::concurrent_hash_map<std::string, ProducerBucket>::accessor;

  /// @brief Use once at the beginning of the program to set the client, which
  /// will be shared iwth the rest of the application. The client is passed as a
  /// shared pointer to ensure that it remains alive at least until this pool is
  /// destroyed.
  /// @param client
  inline void SetClient(std::shared_ptr<pulsar::Client> client) {
    _client = client;
  }

  inline std::shared_ptr<pulsar::Client> GetClient() { return _client; }

  /// @brief Creates a producer to write on the specified topic, and calls the
  /// callback when the producer is ready to be used.
  /// @tparam Callback
  /// @param topic
  /// @param callback
  template <typename Callback>
  void _createProducer(const std::string &topic, Callback callback) {
    ProducerBucket bucket;
    bucket.producer = std::make_shared<pulsar::Producer>();
    bucket.lastUsed = std::chrono::system_clock::now();
    auto result = _client->createProducer(topic, *bucket.producer);
    if (result != pulsar::ResultOk) {
      throw std::runtime_error("Failed to create producer for topic " + topic);
    }
    producer_accessor accessor;
    if (_producers.insert(accessor, topic)) {
      accessor->second = bucket;
      accessor.release();
      callback();
    } else {
      accessor.release();
      if (not _producers.find(accessor, topic)) {
        throw std::runtime_error(
            "Failed to create or find producer for topic " + topic);
      }
    }
  }

  /// @brief Writes the specified request to the specified topic.
  /// @param topic
  /// @param request
  bool write(const std::string &topic, const req::RPRequest &request);

  ///@brief Cleans up the pool by removing unused producers.
  void cleanup();

private:
  std::shared_ptr<pulsar::Client> _client;
  tbb::concurrent_hash_map<std::string, ProducerBucket> _producers;
  std::mutex _mutex;
};

} // namespace detail

template <typename T> struct TypeIdentifier {
  static std::string name() { return "Unknown"; }
};

#define DEFINE_TYPE_IDENTIFIER(type)                                           \
  template <> struct celte::TypeIdentifier<type> {                             \
    static std::string name() { return #type; }                                \
  }

using CStatus = std::optional<std::exception_ptr>;

template <typename... Objects> std::string __serialize__(Objects... objects) {
  nlohmann::json j;
  std::tuple<Objects...> t(objects...);
  std::apply([&j](auto &&...args) { (j.push_back(args), ...); }, t);
  return j.dump();
}

template <typename RetVal> RetVal __deserialize__(const std::string &str) {
  nlohmann::json j = nlohmann::json::parse(str);
  RetVal ret;
  j.get_to(ret);
  return ret;
}

class CRPCTimeoutException : public std::exception {
public:
  CRPCTimeoutException(std::string message) : _message(std::move(message)) {}
  const char *what() const noexcept override { return _message.c_str(); }

private:
  std::string _message;
};

/// @brief The RPCCallerStub class is a singleton that allows to call remote
/// methods on peers. It is thread safe.
class RPCCallerStub {
public:
  ~RPCCallerStub() {
    _producerPool.cleanup();
    _responseConsumer.close();
  }

  /// @brief Calls a remote method on the specified peer, and does not wait
  /// for the result. Returns true if the call was sent successfully, false
  /// otherwise.
  /// @tparam ...Args
  /// @param scope
  /// @param method_name
  /// @param ...args
  template <typename... Args>
  CStatus fire_and_forget(const std::string &scope,
                          const std::string &method_name, Args... args) {
    try {
      req::RPRequest request;
      request.set_name(method_name);
      request.set_args(__serialize__(args...));
      request.set_response_topic("");
      request.set_rpc_id(
          boost::uuids::to_string(boost::uuids::random_generator()()));
      if (not _producerPool.write(scope, request)) {
        return std::make_exception_ptr(
            std::runtime_error("Failed to write to topic " + scope));
      }
    } catch (const std::exception &e) {
      return std::make_exception_ptr(e);
    }
    return std::nullopt;
  }

  /// @brief Sets the pulsar client that will be used to create producers to
  /// write to remote peers. The client is passed as a shared pointer to ensure
  /// that it remains alive at least until this object is destroyed.
  inline void SetClient(std::shared_ptr<pulsar::Client> client) {
    _producerPool.SetClient(client);
  }

  /// @brief Starts a consumer to listen to responses to remote calls. Must be
  /// called after the pulsar client has been initialized and assigned to this
  /// instance of the class.
  void StartListeningForAnswers();

  /// @brief Calls a remote method on the specified scope, returns a future
  /// associated with the result, or an exception if the call failed.
  /// @tparam ...Args
  /// @param scope
  /// @param method_name
  /// @param ...args
  template <typename... Args>
  std::variant<std::exception_ptr, std::future<nlohmann::json>>
  call(const std::string &scope, const std::string &method_name,
       const std::string &rpc_id, Args... args) {
    try {
      req::RPRequest request;
      request.set_name(method_name);
      request.set_args(__serialize__(args...));
      request.set_response_topic(PERSISTENT_DEFAULT + RUNTIME.GetUUID());
      request.set_rpc_id(rpc_id);

      {
        std::lock_guard<std::mutex> lock(_rpcPromisesMutex);
        _promises[rpc_id] = std::make_shared<std::promise<nlohmann::json>>();
        _producerPool.write(scope, request);
        return _promises[rpc_id]->get_future();
      }
    } catch (const std::exception &e) {
      return std::make_exception_ptr(e);
    }
  }

  /// @brief Solves the promise associated with the specified rpc_id.
  /// @param rpc_id
  /// @param response the contents of the reponse to the rpc as a json object.
  void solve_promise(const std::string &rpc_id, nlohmann::json response);

  /// @brief Removes the promise associated with the specified rpc_id.
  /// @param rpc_id
  void forget_promise(const std::string &rpc_id);

  /// @brief Returns the singleton instance of this class.
  static RPCCallerStub &instance();

private:
  std::unordered_map<std::string, std::shared_ptr<std::promise<nlohmann::json>>>
      _promises;                ///< map of rpc_id to promise
  std::mutex _rpcPromisesMutex; ///< mutex to protect the promises map
  detail::ApachePulsarRPCProducerPool _producerPool; ///< pool of producers
  pulsar::Consumer _responseConsumer; ///< consumer to listen to responses to
  ///< our remote calls
};

/// @brief Class that allows to build a remote procedure call,
/// ensuring the necessary setup is performed before calling the remote method.
/// @tparam MetaFunction The type of the remote method to call.
template <typename MetaFunction> class CRPCBuilder {
public:
  class TimeoutPolicy;
  class RetryPolicy;
  class FailHandlingPolicy;
  class CallObject;

  /// @brief The TimeoutPolicy class allows to specify a timeout for the remote
  /// call.
  class TimeoutPolicy {
  public:
    TimeoutPolicy(std::string scope, bool called_on_multiple_peers,
                  std::function<void(CStatus &)> fail_callback)
        : _scope(std::move(scope)), _fail_callback(std::move(fail_callback)),
          _called_on_multiple_peers(std::move(called_on_multiple_peers)) {}

    /// @brief  Specifies the timeout for the remote call, in milliseconds.
    /// If no response is received within the timeout, the call fails and the
    /// fail handler defined in the FailHandlingPolicy is called.
    /// @param timeout
    /// @return
    inline auto with_timeout(std::chrono::milliseconds timeout) {
      return RetryPolicy(std::move(_scope), timeout, _called_on_multiple_peers,
                         _fail_callback);
    }

    /// @brief Fire and forget does not need timeout information. The method
    /// is called immediately.
    /// @tparam ...Args
    /// @param ...args
    template <typename... Args> void fire_and_forget(Args &&...args) {
      CStatus error = RPCCallerStub::instance().fire_and_forget(
          _scope, TypeIdentifier<MetaFunction>::name(), args...);
      if (error.has_value()) {
        _fail_callback(error);
      }
    }

  private:
    std::string _scope;
    bool _called_on_multiple_peers = false;
    std::function<void(CStatus &)> _fail_callback;
  };

  /// @brief The RetryPolicy class allows to specify the number of times the
  /// remote call should be retried in case of failure. Each retry has the same
  /// timeout as the original call.
  class RetryPolicy {
  public:
    RetryPolicy(std::string scope, std::chrono::milliseconds timeout,
                bool called_on_multiple_peers,
                std::function<void(CStatus &)> fail_callback)
        : _scope(std::move(scope)), _timeout(timeout),
          _called_on_multiple_peers(called_on_multiple_peers),
          _fail_callback(std::move(fail_callback)) {}

    /// @brief Specifies the number of times the remote call should be retried
    /// in case of failure.
    inline auto retry(int times) {
      return CallObject(std::move(_scope), _timeout, times, _fail_callback,
                        _called_on_multiple_peers);
    }

  private:
    std::string _scope;
    bool _called_on_multiple_peers = false;
    std::chrono::milliseconds _timeout;
    std::function<void(CStatus &)> _fail_callback;
  };

  /// @brief The FailHandlingPolicy class allows to specify a callback that
  /// should be called in case of failure of the remote call.
  class FailHandlingPolicy {
  public:
    FailHandlingPolicy(std::string scope, bool called_on_multiple_peers)
        : _scope(std::move(scope)),
          _called_on_multiple_peers(called_on_multiple_peers) {}

    /// @brief Specifies the callback that should be called in case of failure
    /// of the remote call. Failure includes either the inability to call the
    /// remote method due to an network error, timeout, or the remote method
    /// itself failing to execute properly.
    inline auto on_fail_do(std::function<void(CStatus &)> fail_callback) {
      return TimeoutPolicy(std::move(_scope), _called_on_multiple_peers,
                           fail_callback);
    }

    /// @brief Logs the error message to redis if the call fails.
    inline auto on_fail_log_error() {
      return TimeoutPolicy(
          std::move(_scope), _called_on_multiple_peers, [](CStatus &status) {
            try {
              if (status) {
                std::rethrow_exception(*status);
              }
            } catch (const std::exception &e) {
              LOGERROR("Remote call failed: " + std::string(e.what()));
              std::cout << "\033[31mRemote call failed: \033[0m" << e.what()
                        << std::endl;
            }
          });
    }

    /// @brief Ignores the error if the call fails.
    inline auto on_fail_ignore() {
      return TimeoutPolicy(std::move(_scope), _called_on_multiple_peers,
                           [](CStatus &status) {});
    }

    /// @brief Throws an exception if the call fails. Avoid using this along
    /// with call_async as the error likely won't be caught or handled
    /// correctly.
    inline auto on_fail_throw() {
      return TimeoutPolicy(std::move(_scope), _called_on_multiple_peers,
                           [](CStatus &status) {
                             if (status) {
                               std::rethrow_exception(*status);
                             }
                           });
    }

    /// @brief Throws an exception if the call fails and the condition is met.
    inline auto on_fail_throw_if(bool condition) {
      return TimeoutPolicy(std::move(_scope), _called_on_multiple_peers,
                           [condition](CStatus &status) {
                             if (status && condition) {
                               std::rethrow_exception(*status);
                             }
                           });
    }

  private:
    std::string _scope;
    bool _called_on_multiple_peers = false;
    std::function<void(CStatus &)> _fail_callback;
  };

  /// @brief The CallObject class allows to specify the remote call to make and
  /// stores the variables of the call until it is actually made.
  class CallObject {
  public:
    CallObject(std::string scope, std::chrono::milliseconds timeout, int retry,
               std::function<void(CStatus &)> fail_callback,
               bool called_on_multiple_peers)
        : _scope(std::move(scope)), _timeout(timeout), _retry(retry),
          _fail_callback(std::move(fail_callback)),
          _called_on_multiple_peers(called_on_multiple_peers) {}

    /// @brief Calls the remote method on the specified peer, and waits for the
    /// result. If the call fails, the fail handler specified in the
    /// FailHandlingPolicy is called.
    /// @tparam RetVal
    /// @tparam ...Args
    /// @param ...args
    /// @return
    template <typename RetVal, typename... Args>
    std::optional<RetVal> call(Args &&...args) {
      return __call_impl<RetVal>(
          boost::uuids::to_string(boost::uuids::random_generator()()), args...);
    }

    /// @brief Calls the remote method on the specified peer, and does not wait
    /// for the result. If the call fails, the fail handler specified in the
    /// FailHandlingPolicy is called. As no result is expected, if the remote
    /// peer receives the message but fails to execute the method, no error
    /// handling is done.
    template <typename... Args> void fire_and_forget(Args &&...args) {
      CStatus error = RPCCallerStub::instance().fire_and_forget(
          _scope, TypeIdentifier<MetaFunction>::name(), args...);
      if (error.has_value()) {
        _fail_callback(error);
      }
    }

    /// @brief Calls the remote method on the specified peer, and waits for the
    /// result. If the call fails, the fail handler specified in the
    /// FailHandlingPolicy is called. The call is made asynchronously and the
    /// result is handled in the callback passed in argument.
    template <typename RetVal, typename... Args>
    void call_async(std::function<void(RetVal)> callback, Args... args) {
      if (_called_on_multiple_peers)
        throw std::logic_error("Cannot call get() on a call to multiple peers");

      // capturing a copy of this to extend its lifetime
      RUNTIME.ScheduleAsyncTask([self = *this, callback, args...]() mutable {
        std::string uuid = RUNTIME.GenUUID();
        std::optional<RetVal> result =
            self.template __call_impl<RetVal>(std::move(uuid), args...);
        if (result.has_value()) {
          callback(result.value());
        }
      });
    }

  private:
    /// @brief Calls the remote method on the specified peer, and waits for the
    /// result. If the call fails, the fail handler specified in the
    /// FailHandlingPolicy is called.
    /// @tparam RetVal
    /// @tparam ...Args
    /// @param uuid
    /// @param ...args
    /// @return
    template <typename RetVal, typename... Args>
    std::optional<RetVal> __call_impl(const std::string &&uuid,
                                      Args &&...args) {
      if (_called_on_multiple_peers)
        throw std::logic_error("Cannot call get() on a call to multiple peers");
      std::variant<std::exception_ptr, std::future<nlohmann::json>> r =
          RPCCallerStub::instance().call(
              _scope, TypeIdentifier<MetaFunction>::name(), uuid, args...);

      // handle failure to call the remote method.
      if (std::holds_alternative<std::exception_ptr>(r)) {
        CStatus status = std::get<std::exception_ptr>(r);
        return __handle_failure<RetVal>(status, std::move(uuid), args...);
      }

      // call successful, but can still timeout
      if (_timeout.count() > 0) {
        std::future_status f_status =
            std::get<std::future<nlohmann::json>>(r).wait_for(
                std::chrono::milliseconds(_timeout));
        if (f_status == std::future_status::timeout) {
          CStatus status = std::make_exception_ptr(CRPCTimeoutException(
              TypeIdentifier<MetaFunction>::name() + " timed out"));
          return __handle_failure<RetVal>(status, std::move(uuid), args...);
        }
      } else {
        std::get<std::future<nlohmann::json>>(r).wait();
      }

      // if the response json is null, it means the remote method failed
      auto j = std::get<std::future<nlohmann::json>>(r).get();
      if (j.is_null()) {
        CStatus status = std::make_exception_ptr(std::runtime_error(
            "Remote error in " + TypeIdentifier<MetaFunction>::name()));
        return __handle_failure<RetVal>(status, std::move(uuid), args...);
      }

      try {
        auto result = __deserialize__<RetVal>(j.dump());
        return result;
      } catch (const std::exception &e) {
        CStatus status = std::make_exception_ptr(e);
        return __handle_failure<RetVal>(status, std::move(uuid), args...);
      }
    }

    /// @brief Handles a failure to call the remote method by either retrying to
    /// call it if the retry count is not exhausted, or calling the fail
    /// handler.
    /// @tparam RetVal
    /// @tparam ...Args
    /// @param status
    /// @param uuid
    /// @param ...args
    /// @return
    template <typename RetVal, typename... Args>
    std::optional<RetVal> __handle_failure(CStatus &status,
                                           const std::string &&uuid,
                                           Args &&...args) {
      if (_retry > 0) {
        --_retry;
        return __call_impl<RetVal>(std::move(uuid), args...);
      } else {
        _fail_callback(status);
        // RPCCallerStub::instance().forget_promise(uuid);
        return std::nullopt;
      }
    }

    std::string _scope;
    bool _called_on_multiple_peers = false;
    std::chrono::milliseconds _timeout;
    int _retry;
    std::function<void(CStatus &)> _fail_callback;
  };

  /// @brief Use this method to specifiy that the method should be called on a
  /// specific peer. The scope will be the name of the peer without extensions.
  /// @param peer
  /// @return
  inline auto on_peer(const std::string &peer) {
    return FailHandlingPolicy(PERSISTENT_DEFAULT + peer, false);
  }

  /// @brief Use this method to specify that the method should be called on
  /// multiple peers. The scope will be the scope passed in argument with ".rpc"
  /// added to it.
  /// @param scope
  /// @return
  inline auto on_scope(const std::string &scope) {
    return FailHandlingPolicy(PERSISTENT_DEFAULT + scope + ".rpc", true);
  }
};

/// @brief The RPCCalleeStub class is a singleton that allows to register
/// methods that can be called remotely. It is thread safe.
class RPCCalleeStub {
public:
  struct ScopeMethods {
    pulsar::Consumer consumer;
    std::unordered_map<std::string,
                       std::function<nlohmann::json(nlohmann::json)>>
        methods;
  };
  using scope_method_accessor =
      tbb::concurrent_hash_map<std::string, ScopeMethods>::accessor;

  /// @brief  Template for generic callables, will be used to find what the
  /// return and arg types of a callable are
  template <typename T> struct FunctionTraits;

  // specialization for function pointers
  template <typename Ret, typename... Args>
  struct FunctionTraits<Ret (*)(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
  };

  // specialization for std::function
  template <typename Ret, typename... Args>
  struct FunctionTraits<std::function<Ret(Args...)>> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
  };

  // specialization for member function pointers
  template <typename ClassType, typename Ret, typename... Args>
  struct FunctionTraits<Ret (ClassType::*)(Args...)> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
  };

  // specialization for const member functions
  template <typename ClassType, typename Ret, typename... Args>
  struct FunctionTraits<Ret (ClassType::*)(Args...) const> {
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
  };

  // specialization for lambdas and other callables
  template <typename Callable>
  struct FunctionTraits : FunctionTraits<decltype(&Callable::operator())> {};

  /// @brief template used to create a delegate to call a method on an object
  /// @tparam Ret
  /// @tparam ArgsTuple
  template <typename Ret, typename ArgsTuple> struct FunctionClassifier;

  // specialization for non-void return type and no arguments
  template <typename Ret> struct FunctionClassifier<Ret, std::tuple<>> {
    using FunctionType = Ret();

    template <typename ClassType>
    static auto build_delegate(ClassType *instance,
                               FunctionType ClassType::*func) {
      return [instance, func](nlohmann::json) -> nlohmann::json {
        Ret result = (instance->*func)();
        return nlohmann::json(result);
      };
    }
  };

  // specialization for void return type and no arguments
  template <> struct FunctionClassifier<void, std::tuple<>> {
    using FunctionType = void();

    template <typename ClassType>
    static auto build_delegate(ClassType *instance,
                               FunctionType ClassType::*func) {
      return [instance, func](nlohmann::json) -> nlohmann::json {
        (instance->*func)();
        return nlohmann::json(); // Return an empty JSON object for void
      };
    }
  };

  // specialization for non-void return type
  template <typename Ret, typename... Args>
  struct FunctionClassifier<Ret, std::tuple<Args...>> {
    using FunctionType = Ret(Args...);

    template <typename ClassType>
    static auto build_delegate(ClassType *instance,
                               FunctionType ClassType::*func) {
      return [instance, func](nlohmann::json jargs) -> nlohmann::json {
        std::tuple<Args...> args;
        jargs.get_to(args);

        Ret result = std::apply(
            [instance, func](Args... unpackedArgs) {
              return (instance->*func)(unpackedArgs...);
            },
            args);
        return nlohmann::json(result);
      };
    }
  };

  // specialization for void return type
  template <typename... Args>
  struct FunctionClassifier<void, std::tuple<Args...>> {
    using FunctionType = void(Args...);

    template <typename ClassType>
    static auto build_delegate(ClassType *instance,
                               FunctionType ClassType::*func) {
      return [instance, func](nlohmann::json jargs) -> nlohmann::json {
        std::tuple<Args...> args;
        jargs.get_to(args);
        std::apply(
            [instance, func](Args... unpackedArgs) {
              (instance->*func)(unpackedArgs...);
            },
            args);
        return nlohmann::json(); // Return an empty JSON object for void
      };
    }
  };

  /// @brief Register a method that can be called by another remote instance of
  /// the program. Note that this method is blocking and might perform IO
  /// operations to create a consumer on the specified scope.
  template <class BoundClass, typename Func>
  void register_method(BoundClass *instance, const std::string &scope,
                       const std::string &method_name, Func &&method) {
    using Traits = FunctionTraits<std::decay_t<Func>>;
    using RetVal = typename Traits::ReturnType;
    using ArgsTuple = typename Traits::ArgsTuple;

    // build a delegate to abstract the type of the method
    auto call_delegate =
        FunctionClassifier<RetVal, ArgsTuple>::build_delegate(instance, method);

    scope_method_accessor accessor;
    if (_methods.find(accessor, scope)) {
      accessor->second.methods.insert({method_name, call_delegate});
    } else {
      ScopeMethods scope_methods;
      scope_methods.methods.insert({method_name, call_delegate});
      _methods.insert(accessor, scope);
      accessor->second = std::move(scope_methods);
      _init_consumer(scope, accessor->second.consumer);
    }
  }

  /// @brief Set the client that will be used to create consumers to listen to
  /// remote calls. The client is passed as a shared pointer to ensure that it
  /// remains alive at least until this object is destroyed.
  inline void SetClient(std::shared_ptr<pulsar::Client> client) {
    _client = client;
    _producerPool.SetClient(client);
  }

  /// @brief Initializes the consumer for the specified scope. The consumer is
  /// configured to listen to the scope, and the message listener is set to
  /// handle the incoming messages by calling the appropriate rpc handler.
  /// @param scope
  /// @param consumer
  void _init_consumer(const std::string &scope, pulsar::Consumer &consumer);

  /// @brief Unregisters the method with the specified name on the specified
  /// scope.
  void unregister_method(const std::string &scope,
                         const std::string &method_name);
  /// @brief Handles the incoming request by calling the appropriate handler.
  /// If the request is actually a response to a previously made request, the
  /// response is handled differently and the promise is solved.
  /// @param scope
  void try_handle_request(const std::string &scope,
                          const req::RPRequest &request);

  /// @brief Handles a response to a previously made request.
  /// @param request
  void _handle_response(const req::RPRequest &request);

  /// @brief Handles a call made remotely to a local method.
  void _handle_call(const std::string &scope, const req::RPRequest &request);

  /// @brief Returns the singleton instance of this class.
  static RPCCalleeStub &instance();

private:
  tbb::concurrent_hash_map<std::string, ScopeMethods> _methods;
  std::shared_ptr<pulsar::Client> _client;
  detail::ApachePulsarRPCProducerPool _producerPool;
  detail::UniqueTaskManager _uniqueTaskManager;
};

} // namespace celte

// Helper to extract argument types from a member function pointer
template <typename T> struct function_traits;

// Specialization for member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...)> {
  using arg_types = std::tuple<Args...>;
};

// Specialization for const member functions
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType (ClassType::*)(Args...) const> {
  using arg_types = std::tuple<Args...>;
};

#define REGISTER_RPC_CALL_STUB(bound_class, method_name)                       \
  struct Call##bound_class##method_name;                                       \
  template <> struct celte::TypeIdentifier<Call##bound_class##method_name> {   \
    static std::string name() {                                                \
      return std::string(#bound_class) + "_call_" + #method_name;              \
    }                                                                          \
  };                                                                           \
  struct Call##bound_class##method_name                                        \
      : public celte::CRPCBuilder<Call##bound_class##method_name> {            \
    struct Options {                                                           \
      std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);     \
      int retry = 0;                                                           \
      std::function<void(celte::CStatus &)> fail_callback = [](celte::CStatus  \
                                                                   &) {        \
        LOGERROR(                                                              \
            "RPC failed : " +                                                  \
            celte::TypeIdentifier<Call##bound_class##method_name>::name());    \
        std::cerr                                                              \
            << "\033[1;31mRPC failed : "                                       \
            << celte::TypeIdentifier<Call##bound_class##method_name>::name()   \
            << "\033[0m" << std::endl;                                         \
      };                                                                       \
    };                                                                         \
    Call##bound_class##method_name() = default;                                \
    Call##bound_class##method_name(const Options &options)                     \
        : _options(options) {}                                                 \
                                                                               \
    template <typename RetVal, typename... Args>                               \
    std::optional<RetVal> call_on_peer(const std::string &peer,                \
                                       Args &&...args) {                       \
      if (_options == std::nullopt)                                            \
        throw std::runtime_error("Options not set");                           \
      return on_peer(peer)                                                     \
          .on_fail_do(_options->fail_callback)                                 \
          .with_timeout(_options->timeout)                                     \
          .retry(_options->retry)                                              \
          .call<RetVal>(std::forward<Args>(args)...);                          \
    }                                                                          \
    template <typename RetVal, typename... Args>                               \
    void call_async_on_peer(const std::string &peer,                           \
                            std::function<void(RetVal)> callback,              \
                            Args &&...args) {                                  \
      if (_options == std::nullopt)                                            \
        throw std::runtime_error("Options not set");                           \
      return on_peer(peer)                                                     \
          .on_fail_do(_options->fail_callback)                                 \
          .with_timeout(_options->timeout)                                     \
          .retry(_options->retry)                                              \
          .call_async<RetVal>(callback, std::forward<Args>(args)...);          \
    }                                                                          \
    template <typename... Args>                                                \
    void fire_and_forget_on_peer(const std::string &peer, Args &&...args) {    \
      if (_options == std::nullopt)                                            \
        throw std::runtime_error("Options not set");                           \
      return on_peer(peer)                                                     \
          .on_fail_do(_options->fail_callback)                                 \
          .fire_and_forget(std::forward<Args>(args)...);                       \
    }                                                                          \
    template <typename... Args>                                                \
    void fire_and_forget_on_scope(const std::string &scope, Args &&...args) {  \
      if (_options == std::nullopt)                                            \
        throw std::runtime_error("Options not set");                           \
      on_scope(scope)                                                          \
          .on_fail_do(_options->fail_callback)                                 \
          .fire_and_forget(std::forward<Args>(args)...);                       \
    }                                                                          \
                                                                               \
  private:                                                                     \
    std::optional<Options> _options;                                           \
  };

#define REGISTER_RPC_REACTOR(bound_class, method_name)                         \
  class bound_class##method_name##Reactor {                                    \
  private:                                                                     \
    using MethodType = decltype(&bound_class::method_name);                    \
    using Traits = celte::RPCCalleeStub::FunctionTraits<MethodType>;           \
    using ReturnType = typename Traits::ReturnType;                            \
    using ArgsTuple = typename Traits::ArgsTuple;                              \
                                                                               \
  public:                                                                      \
    static void subscribe(const std::string &topic, bound_class *instance) {   \
      celte::RPCCalleeStub::instance().register_method(                        \
          instance, topic,                                                     \
          celte::TypeIdentifier<Call##bound_class##method_name>::name(),       \
          &bound_class::method_name);                                          \
    }                                                                          \
                                                                               \
    static void unsubscribe(const std::string &topic) {                        \
      celte::RPCCalleeStub::instance().unregister_method(                      \
          topic,                                                               \
          celte::TypeIdentifier<Call##bound_class##method_name>::name());      \
    }                                                                          \
  };

#ifdef CELTE_SERVER_MODE_ENABLED
#define REGISTER_SERVER_RPC(bound_class, method_name)                          \
  REGISTER_RPC_CALL_STUB(bound_class, method_name)                             \
  REGISTER_RPC_REACTOR(bound_class, method_name)

#define REGISTER_CLIENT_RPC(bound_class, method_name)                          \
  REGISTER_RPC_CALL_STUB(bound_class, method_name)
#else
#define REGISTER_SERVER_RPC(bound_class, method_name)                          \
  REGISTER_RPC_CALL_STUB(bound_class, method_name)

#define REGISTER_CLIENT_RPC(bound_class, method_name)                          \
  REGISTER_RPC_CALL_STUB(bound_class, method_name)                             \
  REGISTER_RPC_REACTOR(bound_class, method_name)
#endif

/// @brief Registers a method that can be called remotely both on clients and
/// servers.
#define REGISTER_RPC(bound_class, method_name)                                 \
  REGISTER_RPC_CALL_STUB(bound_class, method_name)                             \
  REGISTER_RPC_REACTOR(bound_class, method_name)