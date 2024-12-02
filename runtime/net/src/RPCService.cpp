#include "RPCService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"

namespace celte {
namespace net {
RPCService::RPCService(const RPCService::Options &options)
    : _work(_io), _options(options),
      _writerStreamPool(
          WriterStreamPool::Options{.idleTimeout =
                                        std::chrono::milliseconds(10000)},
          _io) {
  for (int i = 0; i < options.nThreads; i++) {
    _threads.create_thread([this]() { _io.run(); });
  }

  if (options.listenOn.size() != 0) {
    std::cout << "calling init reader stream" << std::endl;
    __initReaderStream(options.listenOn);
  }
}

void RPCService::__initReaderStream(const std::vector<std::string> &topic) {
  _createReaderStream<RPRequest>(
      {.thisPeerUuid = _options.thisPeerUuid,
       .topics = {topic},
       .subscriptionName = "",
       .exclusive = false,
       .messageHandlerSync =
           [this](const pulsar::Consumer, RPRequest req) {
             if (!req.respondsTo.empty()) {
               return; // nothing to do, handled in async handler
             }
             __handleRemoteCall(req);
           },
       .messageHandler =
           [this](const pulsar::Consumer, RPRequest req) {
             if (!req.respondsTo.empty()) {
               std::cout << "handling response" << std::endl;
               __handleResponse(req);
             }
           }});
}

void RPCService::__handleRemoteCall(const RPRequest &req) {
  std::cout << "handling remote call " << req.name << std::endl;
  auto it = _rpcs.find(req.name);
  if (it == _rpcs.end()) {
    // no such rpc
    std::cerr << "No such rpc: " << req.name << std::endl;
    return;
  }

  auto f = it->second;
  auto result = f(req.args);
  std::cout << "sending response to topic " << req.responseTopic << std::endl;
  RPRequest response{
      .name = req.name,
      .respondsTo = req.rpcId,
      .responseTopic = "", // can't respond to a response
      .rpcId = boost::uuids::to_string(boost::uuids::random_generator()()),
      .args = result,
  };

  if (not req.responseTopic.empty()) {
    _writerStreamPool.Write(req.responseTopic, response);
  }
}

void RPCService::__handleResponse(const RPRequest &req) {
  auto it = rpcPromises.find(req.respondsTo);
  if (it == rpcPromises.end()) {
    // no such promise
    std::cerr << "No such promise: " << req.respondsTo << std::endl;
    return;
  }

  std::cout << "setting promise value" << std::endl;
  auto promise = it->second;
  promise->set_value(req.args.dump());
  std::cout << "value is set " << std::endl;
  rpcPromises.erase(it);
}

} // namespace net
} // namespace celte