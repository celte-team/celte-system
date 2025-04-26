#include "CRPC.hpp"
#include "Topics.hpp"
#include <google/protobuf/util/json_util.h>

namespace celte {
namespace detail {
/**
 * @brief Sends an RPC request asynchronously over Apache Pulsar.
 *
 * This function attempts to locate an existing producer for the specified topic. If a producer exists,
 * it updates the producer's last-used timestamp, serializes the RPC request into JSON, and sends it asynchronously.
 * In case of serialization failure, it logs an error and aborts the send operation. If no producer is found,
 * it initiates the creation of a new producer and retries sending the request.
 *
 * @param topic The Apache Pulsar topic to which the RPC request should be sent.
 * @param request The RPC request message to be transmitted.
 */
void ApachePulsarRPCProducerPool::write(const std::string &topic,
                                        const req::RPRequest &request) {
  producer_accessor accessor;

  if (_producers.find(accessor, topic)) {
    accessor->second.lastUsed = std::chrono::system_clock::now();
    std::string msgStr;
    if (not google::protobuf::util::MessageToJsonString(request, &msgStr)
                .ok()) {
      std::cerr << "Error while serializing request." << std::endl;
      return false;
    }
    accessor->second.producer->sendAsync(
        pulsar::MessageBuilder().setContent(msgStr).build(),
        [this, topic](pulsar::Result result, pulsar::MessageId messageId) {
          if (result != pulsar::ResultOk) {
            std::cout << "producer failed to send message. reason: "
                      << pulsar::strResult(result) << std::endl;
          }
        });
    return true;
  } else {
    accessor.release();

    try {
      _createProducer(topic,
                      [this, topic, request]() { write(topic, request); });
      return true;
    } catch (const std::exception &e) {
      std::cerr << "Failed to create producer for topic " << topic << ": "
                << e.what() << std::endl;
      return false;
    }
  }
}

/**
 * @brief Removes producers that haven't been used for more than 60 seconds.
 *
 * Iterates through the internal producers map and erases any entry whose
 * last interaction occurred over 60 seconds ago, thereby freeing up resources
 * by cleaning up stale producers.
 */
void ApachePulsarRPCProducerPool::cleanup() {
  std::chrono::time_point<std::chrono::system_clock> now =
      std::chrono::system_clock::now();
  std::vector<std::string> toRemove;
  for (auto it = _producers.begin(); it != _producers.end(); ++it) {
    if (now - it->second.lastUsed > std::chrono::seconds(60)) {
      toRemove.push_back(it->first);
    }
  }
  for (const std::string &topic : toRemove) {
    producer_accessor accessor;
    if (_producers.find(accessor, topic)) {
      _producers.erase(accessor);
    }
  }
}

} /**
 * @brief Retrieves the singleton instance of RPCCalleeStub.
 *
 * This function returns a reference to a unique, statically initialized instance of RPCCalleeStub,
 * ensuring that only one instance is active during the application's lifecycle.
 *
 * @return RPCCalleeStub& Reference to the singleton instance.
 */

RPCCalleeStub &RPCCalleeStub::instance() {
  static RPCCalleeStub instance;
  return instance;
}

/**
 * @brief Starts listening for incoming RPC responses.
 *
 * Configures a Pulsar consumer with a shared consumer type and attaches a message listener
 * that processes each incoming message. The listener attempts to parse the message's data as a JSON-formatted
 * RPC request. If parsing is successful, the message is acknowledged and the request is forwarded to the
 * callee stub for handling; otherwise, an error is logged. Finally, the client subscribes to a topic based
 * on the runtime's unique identifier to receive RPC responses.
 */
void RPCCallerStub::StartListeningForAnswers() {
  pulsar::ConsumerConfiguration consumerConfig;
  consumerConfig.setConsumerType(pulsar::ConsumerShared);
  consumerConfig.setMessageListener([this](pulsar::Consumer &consumer,
                                           const pulsar::Message &msg) {
    req::RPRequest request;
    std::string data(static_cast<const char *>(msg.getData()), msg.getLength());
    if (!google::protobuf::util::JsonStringToMessage(data, &request).ok()) {
      std::cerr << "Failed to parse message: " << msg.getDataAsString()
                << std::endl;
      return;
    }
    consumer.acknowledge(msg);
    RPCCalleeStub::instance().try_handle_request(
        tp::default_scope + RUNTIME.GetUUID(), request);
  });

  std::string subscriptionName = RUNTIME.GetUUID();
  pulsar::Result result = _producerPool.GetClient()->subscribe(
      RUNTIME.GetUUID(), subscriptionName, consumerConfig, _responseConsumer);
}

/**
 * @brief Resolves an RPC promise using the provided JSON response.
 *
 * This function checks for the existence of a promise associated with the given RPC identifier.
 * If found, it fulfills the promise by setting its value to the provided JSON response and then
 * removes the promise from the internal tracking map. If no promise with the specified identifier exists,
 * the function exits without performing any operation.
 *
 * @param rpc_id The unique identifier for the RPC call.
 * @param response The JSON response to be used for fulfilling the promise.
 */
void RPCCallerStub::solve_promise(const std::string &rpc_id,
                                  nlohmann::json response) {
  std::shared_ptr<std::promise<nlohmann::json>> promise;
  {
    std::lock_guard<std::mutex> lock(_rpcPromisesMutex);
    if (_promises.find(rpc_id) == _promises.end()) {
      return;
    }
    promise = _promises[rpc_id];
    promise->set_value(response);
    _promises.erase(rpc_id);
  }
}

/**
 * @brief Clears the promise for the specified RPC call.
 *
 * If a promise associated with the given RPC ID exists, this method sets a runtime error exception on it to indicate that it was manually cleared, and then removes it from the internal promise map.
 *
 * @param rpc_id Identifier for the RPC call whose promise is to be forgotten.
 */
void RPCCallerStub::forget_promise(const std::string &rpc_id) {
  std::lock_guard<std::mutex> lock(_rpcPromisesMutex);
  if (_promises.find(rpc_id) == _promises.end()) {
    return;
  }
  _promises[rpc_id]->set_exception(std::make_exception_ptr(
      std::runtime_error("Promise for " + rpc_id + " was destroyed manually")));
  _promises.erase(rpc_id);
}

/**
 * @brief Retrieves the singleton instance of RPCCallerStub.
 *
 * This method ensures that only one instance of RPCCallerStub exists by
 * returning a reference to a static local instance. It provides a centralized
 * point of access for managing RPC caller functions throughout the application.
 *
 * @return RPCCallerStub& Reference to the singleton RPCCallerStub instance.
 */
RPCCallerStub &RPCCallerStub::instance() {
  static RPCCallerStub instance;
  return instance;
}

/**
 * @brief Configures and subscribes a Pulsar consumer to a specified scope.
 *
 * This method sets up the consumer to operate in shared mode and installs a message listener
 * that acknowledges messages, deserializes them from JSON into an RPRequest, and delegates further
 * processing via the request handler. It generates a unique subscription name at runtime and
 * subscribes the consumer to the provided scope.
 *
 * @param scope The topic or scope to which the consumer subscribes for receiving RPC requests.
 * @param consumer The Pulsar consumer instance that will be initialized with the configured listener.
 *
 * @throws std::runtime_error If the subscription to the given scope fails.
 */
void RPCCalleeStub::_init_consumer(const std::string &scope,
                                   pulsar::Consumer &consumer) {
  pulsar::ConsumerConfiguration consumerConfig;
  consumerConfig.setConsumerType(pulsar::ConsumerShared);
  consumerConfig.setMessageListener([this, scope](pulsar::Consumer &consumer,
                                                  const pulsar::Message &msg) {
    std::string payload = msg.getDataAsString();
    consumer.acknowledge(msg);
    req::RPRequest request;
    std::string data(static_cast<const char *>(msg.getData()), msg.getLength());
    if (!google::protobuf::util::JsonStringToMessage(data, &request).ok()) {
      std::cerr << "Failed to parse message: " << msg.getDataAsString()
                << std::endl;
      return;
    }
    try_handle_request(scope, request);
  });

  std::string subscriptionName = RUNTIME.GetUUID();
  pulsar::Result result =
      _client->subscribe(scope, subscriptionName, consumerConfig, consumer);
  if (result != pulsar::ResultOk) {
    throw std::runtime_error("Failed to subscribe to " + scope + ": " +
                             pulsar::strResult(result));
  }
}

/**
 * @brief Unregisters an RPC method from the specified scope.
 *
 * Removes the method identified by the given name from the registry of methods associated with the provided scope.
 * If the scope does not exist or the method is not registered under that scope, the function performs no action.
 *
 * @param scope The RPC scope identifier.
 * @param method_name The name of the method to unregister.
 */
void RPCCalleeStub::unregister_method(const std::string &scope,
                                      const std::string &method_name) {
  scope_method_accessor accessor;
  if (_methods.find(accessor, scope)) {
    accessor->second.methods.erase(method_name);
  }
}
/**
 * @brief Processes an incoming RPC request.
 *
 * This function checks if a request with the same unique identifier has already been
 * processed to prevent duplicate handling. It then delegates the request to the appropriate
 * handler: if the request is a response (its "responds_to" field is non-empty), it is handled
 * as a response; otherwise, it is treated as an RPC call within the provided scope.
 *
 * @param scope A string representing the operational context for the RPC call.
 * @param request The RPC request containing the details to be processed, including its unique ID.
 */
void RPCCalleeStub::try_handle_request(const std::string &scope,
                                       const req::RPRequest &request) {
  // do not run the handler if we have already processed an rpc with the same
  // uuid, to avoid repeating the same operation.
  _uniqueTaskManager.run(request.rpc_id(), [this, scope, request]() {
    if (request.responds_to().length() > 0) {
      _handle_response(request);
    } else {
      _handle_call(scope, request);
    }
  });
}

/**
 * @brief Processes an RPC response and resolves the corresponding promise.
 *
 * This method checks if the incoming RPC response indicates an error. If an error is present,
 * it resolves the associated promise with an empty response. Otherwise, it deserializes the response
 * arguments from a JSON string and resolves the promise with the parsed data. If JSON parsing fails,
 * the promise is resolved with an empty response.
 *
 * @param request The RPC request containing the response to be handled.
 */
void RPCCalleeStub::_handle_response(const req::RPRequest &request) {
  if (request.error_status()) { // remote error
    RPCCallerStub::instance().solve_promise(request.responds_to(), nullptr);
    return;
  }
  try { // unpack the response and solve the promise
    nlohmann::json args = nlohmann::json::parse(request.args());
    RPCCallerStub::instance().solve_promise(request.responds_to(), args);
  } catch (nlohmann::json::exception &e) { // handle bad json values
    RPCCallerStub::instance().solve_promise(request.responds_to(), nullptr);
    return;
  }
}

/**
 * @brief Handles an incoming RPC call request by invoking the corresponding registered method.
 *
 * The method locates the handler for the given request within the specified scope, parses
 * the JSON arguments, and executes the handler. It captures any exceptions raised during
 * invocation to prepare an appropriate error response. If the request specifies a response
 * topic, the response (or error message) is sent back; otherwise, no response is returned.
 *
 * @param scope The scope used to look up the registered methods.
 * @param request The RPC request containing the method name, serialized arguments, and routing details.
 */
void RPCCalleeStub::_handle_call(const std::string &scope,
                                 const req::RPRequest &request) {
  scope_method_accessor accessor;
  if (not _methods.find(accessor, scope)) {
    std::cerr << "rpc scope " << scope << " not found, cannot handle request"
              << std::endl;
    return;
  }
  req::RPRequest responseRequest;
  nlohmann::json response;
  try {
    // copy construction of the lambda, so we can release the accessor
    // faster
    auto handler = accessor->second.methods.at(request.name());
    accessor.release();
    nlohmann::json args = nlohmann::json::parse(request.args());
    response = handler(args);
  } catch (std::out_of_range &e) {
#ifdef DEBUG
    std::cout << "rpc method " << request.name() << " not found in scope "
              << scope << std::endl;
#endif
    response = "method not found";
    responseRequest.set_error_status(1);
  } catch (nlohmann::json::exception &e) {
#ifdef DEBUG
    std::cout << "error while parsing json: " << e.what() << std::endl;
    std::cout << "json was: " << request.args() << std::endl;
    std::cout << "method is " << request.name() << std::endl;
#endif
    response = std::string(e.what());
    responseRequest.set_error_status(1);
  } catch (std::exception &e) {
#ifdef DEBUG
    std::cout << "there was an exception: " << e.what() << std::endl;
    std::cout << "json was: " << request.args() << std::endl;
    std::cout << "method is " << request.name() << std::endl;
#endif
    response = e.what();
    responseRequest.set_error_status(1);
  }

  if (request.response_topic().empty()) {
    return; // not responding to a call if responds_to is empty
  }
  responseRequest.set_args(response.dump());
  responseRequest.set_name(request.name());
  responseRequest.set_responds_to(request.rpc_id());
  responseRequest.set_rpc_id(request.rpc_id());
  _producerPool.write(request.response_topic(), responseRequest);
}

} // namespace celte


// functional test below!

class Test {
public:
  /**
 * @brief Constructs a Test object with a specified name.
 *
 * Initializes a Test instance using the provided name, which can be used for identification
 * in RPC operations or logging.
 *
 * @param name The name assigned to the Test instance.
 */
Test(const std::string name) : _name(name) {}
  /**
   * @brief Computes the sum of two integers while logging the operation.
   *
   * This method outputs a log message that includes the object's name and the two numbers being added, then returns their sum.
   *
   * @param a The first integer value.
   * @param b The second integer value.
   * @return int The sum of a and b.
   */
  int Add(int a, int b) {
    std::cout << _name << " adding " << a << " and " << b << std::endl;
    return a + b;
  };
  /**
 * @brief Prints a greeting message.
 *
 * Outputs "hello!" to the standard output stream, followed by a newline.
 */
void PrintHello() { std::cout << "hello! " << std::endl; }
  /**
 * @brief Returns the constant integer 5.
 *
 * This function always returns the fixed value of 5.
 *
 * @return int The integer 5.
 */
int GetFive() { return 5; }

private:
  std::string _name;
};

REGISTER_RPC(Test, Add);
REGISTER_RPC(Test, PrintHello);
REGISTER_RPC(Test, GetFive);

/**
 * @brief Executes a series of RPC calls and handles their outcomes.
 *
 * This function performs multiple RPC interactions:
 * - Sends a non-blocking call to invoke a greeting message on "peer1".
 * - Initiates an asynchronous call to retrieve a fixed value (expected to be five) on "peer1", printing the result via a callback.
 * - Performs synchronous addition RPC calls on "peer1" and "peer2", printing the results.
 *
 * Each RPC call is configured with custom error handling that logs exceptions if the call fails.
 * After dispatching these RPC calls, the function enters a loop that reads from standard input,
 * terminating when the user types "exit".
 */
void RunCaller() {

  CallTestPrintHello()
      .on_peer("peer1")
      .on_fail_do([](celte::CStatus &status) {
        try {
          if (status) {
            std::rethrow_exception(*status);
          }
        } catch (const std::exception &e) {
          std::cout << "Failed to call PrintHello: " << e.what() << std::endl;
        }
      })
      .fire_and_forget();

  CallTestGetFive()
      .on_peer("peer1")
      .on_fail_do([](celte::CStatus &status) {
        try {
          if (status) {
            std::rethrow_exception(*status);
          }
        } catch (const std::exception &e) {
          std::cout << "Failed to call GetFive: " << e.what() << std::endl;
        }
      })
      .with_timeout(std::chrono::milliseconds(1000))
      .retry(1)
      .call_async<int>(
          [](int x) { std::cout << "GetFive returned : " << x << std::endl; });

  int x = CallTestAdd()
              .on_peer("peer1")
              .on_fail_do([](celte::CStatus &status) {
                try {
                  if (status) {
                    std::rethrow_exception(*status);
                  }
                } catch (const std::exception &e) {
                  std::cout << "Failed to call Add: " << e.what() << std::endl;
                }
              })
              .with_timeout(std::chrono::milliseconds(1000))
              .retry(1)
              .call<int>(1, 2)
              .value_or(0);
  std::cout << "Result for instance 1: " << x << std::endl;

  int y = CallTestAdd()
              .on_peer("peer2")
              .on_fail_do([](celte::CStatus &status) {
                try {
                  if (status) {
                    std::rethrow_exception(*status);
                  }
                } catch (const std::exception &e) {
                  std::cout << "Failed to call Add: " << e.what() << std::endl;
                }
              })
              .with_timeout(std::chrono::milliseconds(1000))
              .retry(1)
              .call<int>(1, 2)
              .value_or(0);
  std::cout << "Result instance 2: " << y << std::endl;

  while (std::cin) {
    std::string line;
    std::getline(std::cin, line);
    if (line == "exit") {
      break;
    }
  }
}

/**
 * @brief Runs the RPC callee process by subscribing test instances to RPC reactors.
 *
 * This function creates two test objects and registers them with specific RPC reactors:
 * - The first instance ("instance 1") is subscribed to addition, greeting, and fixed integer reactors on peer "peer1".
 * - The second instance ("instance 2") is subscribed to the addition reactor on peer "peer2".
 *
 * After setting up the subscriptions, the function waits for user input from standard input. When the user types "exit",
 * the function breaks out of the loop and unsubscribes all reactors to clean up.
 */
void RunCallee() {
  Test test("instance 1");
  TestAddReactor::subscribe("peer1", &test);
  TestPrintHelloReactor::subscribe("peer1", &test);
  TestGetFiveReactor::subscribe("peer1", &test);

  Test test2("instance 2");
  TestAddReactor::subscribe("peer2", &test2);

  while (std::cin) {
    std::string line;
    std::getline(std::cin, line);
    if (line == "exit") {
      break;
    }
  }

  TestAddReactor::unsubscribe("peer1");
  TestPrintHelloReactor::unsubscribe("peer1");
  TestGetFiveReactor::unsubscribe("peer1");

  TestAddReactor::unsubscribe("peer2");
}

/**
 * @brief Initializes the RPC system and executes the appropriate caller or callee mode.
 *
 * The function configures an Apache Pulsar client with predefined settings and assigns it to both the callee and caller RPC stubs.
 * It then starts listening for RPC responses. If the command-line argument "--call" is provided, the system runs in caller mode,
 * otherwise it operates in callee mode.
 *
 * @param ac Number of command-line arguments.
 * @param av Array of command-line argument strings.
 * @return int Always returns 0.
 */
int test(int ac, char **av) {
  if (ac > 1 && std::string(av[1]) == "--call")
    static_uuid = "caller-peer";

  // creating the pulsar client
  pulsar::ClientConfiguration conf;
  conf.setOperationTimeoutSeconds(10000 / 1000);
  conf.setIOThreads(1);
  conf.setMessageListenerThreads(1);
  conf.setUseTls(false);
  conf.setLogger(new pulsar::ConsoleLoggerFactory(pulsar::Logger::LEVEL_WARN));

  std::string pulsarBrokers = "pulsar://localhost:6650";
  auto client = std::make_shared<pulsar::Client>(pulsarBrokers, conf);
  // wait until the client is connected to the cluster

  celte::RPCCalleeStub::instance().SetClient(client);
  celte::RPCCallerStub::instance().SetClient(client);
  celte::RPCCallerStub::instance().StartListeningForAnswers();

  // if --call in args, we are calling the method.. Else, we are subscribers
  if (ac > 1 && std::string(av[1]) == "--call") {
    RunCaller();
  } else {
    RunCallee();
  }
  return 0;
}
