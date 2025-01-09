#include "RPCService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"

namespace celte {
namespace net {
std::unordered_map<std::string, std::shared_ptr<std::promise<std::string>>>
    RPCService::rpcPromises;
std::mutex RPCService::rpcPromisesMutex;

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
    __initReaderStream(options.listenOn);
  }
}

void RPCService::__initReaderStream(const std::vector<std::string> &topic) {
  _createReaderStream<RPRequest>(
      {.thisPeerUuid = _options.thisPeerUuid,
       .topics = {topic},
       .subscriptionName = _options.serviceName,
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
               __handleResponse(req);
             }
           }});
}

void RPCService::__handleRemoteCall(const RPRequest &req) {
  auto it = _rpcs.find(req.name);
  if (it == _rpcs.end()) {
    // no such rpc
    std::cerr << "No such rpc: " << req.name << std::endl;
    return;
  }
  auto f = it->second;
  auto result = f(req.args);

  if (not req.responseTopic.empty()) {
    RPRequest response{
        .name = req.name,
        .respondsTo = req.rpcId,
        .responseTopic = "",
        .rpcId = boost::uuids::to_string(boost::uuids::random_generator()()),
        .args = result,
    };
    _writerStreamPool.Write(req.responseTopic, response);
  }
}

void RPCService::__handleResponse(const RPRequest &req) {
  std::lock_guard<std::mutex> lock(rpcPromisesMutex);
  auto it = rpcPromises.find(req.respondsTo);
  if (it == rpcPromises.end()) {
    // no such promise
    std::cerr << "No such promise: " << req.respondsTo << std::endl;
    std::cerr << "\tname: " << req.name << std::endl;
    std::cerr << "\trespondsTo: " << req.respondsTo << std::endl;
    std::cerr << "\tresponseTopic: " << req.responseTopic << std::endl;
    return;
  }

  auto promise = it->second;
  promise->set_value(req.args.dump());
  rpcPromises.erase(it);
}

} // namespace net
} // namespace celte