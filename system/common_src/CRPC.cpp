#include "CRPC.hpp"
#include "Topics.hpp"
#include <google/protobuf/util/json_util.h>

namespace celte {
namespace detail {
bool ApachePulsarRPCProducerPool::write(const std::string &topic,
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

} // namespace detail

RPCCalleeStub &RPCCalleeStub::instance() {
  static RPCCalleeStub instance;
  return instance;
}

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
    RUNTIME.GetUUID(), subscriptionName, consumerConfig,
      _responseConsumer);
}

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

void RPCCallerStub::forget_promise(const std::string &rpc_id) {
  std::lock_guard<std::mutex> lock(_rpcPromisesMutex);
  if (_promises.find(rpc_id) == _promises.end()) {
    return;
  }
  _promises[rpc_id]->set_exception(std::make_exception_ptr(
      std::runtime_error("Promise for " + rpc_id + " was destroyed manually")));
  _promises.erase(rpc_id);
}

RPCCallerStub &RPCCallerStub::instance() {
  static RPCCallerStub instance;
  return instance;
}

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

void RPCCalleeStub::unregister_method(const std::string &scope,
                                      const std::string &method_name) {
  scope_method_accessor accessor;
  if (_methods.find(accessor, scope)) {
    accessor->second.methods.erase(method_name);
  }
}
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
