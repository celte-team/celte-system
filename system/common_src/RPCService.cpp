#include "RPCService.hpp"
#include "ReaderStream.hpp"
#include "WriterStream.hpp"
#include "systems_structs.pb.h"

namespace celte {
namespace net {
std::unordered_map<std::string, std::shared_ptr<std::promise<std::string>>>
    RPCService::rpcPromises;
std::mutex RPCService::rpcPromisesMutex;

RPCService::RPCService(const RPCService::Options &options)
    : _options(options), _writerStreamPool(WriterStreamPool::Options{
                             .idleTimeout = std::chrono::milliseconds(10000)}) {
  if (options.listenOn.size() != 0) {
    __initReaderStream(options.listenOn);
  }
}

void RPCService::__initReaderStream(const std::vector<std::string> &topic) {
  _createReaderStream<req::RPRequest>(
      {.thisPeerUuid = _options.thisPeerUuid,
       .topics = {topic},
       .subscriptionName = _options.serviceName,
       .exclusive = false,
       .messageHandlerSync =
           [this](const pulsar::Consumer, req::RPRequest req) {
             if (!req.responds_to().empty()) {
               return; // nothing to do, handled in async handler
             }
             __handleRemoteCall(req);
           },
       .messageHandler =
           [this](const pulsar::Consumer, req::RPRequest req) {
             if (!req.responds_to().empty()) {
               __handleResponse(req);
             }
           }});
}

void RPCService::__handleRemoteCall(const req::RPRequest &req) {
  auto it = _rpcs.find(req.name());
  if (it == _rpcs.end()) {
    // no such rpc
    std::cerr << "No such rpc: " << req.name() << std::endl;
    return;
  }
  auto f = it->second;
  nlohmann::json argsJson = nlohmann::json::parse(req.args());
  auto result = f(argsJson).dump();

  if (not req.response_topic().empty()) {
    req::RPRequest response;
    response.set_name(req.name());
    response.set_responds_to(req.rpc_id());
    response.set_response_topic("");
    response.set_rpc_id(
        boost::uuids::to_string(boost::uuids::random_generator()()));
    response.set_args(result);

    _writerStreamPool.Write(req.response_topic(), response);
  }
}

void RPCService::__handleResponse(const req::RPRequest &req) {
  std::lock_guard<std::mutex> lock(rpcPromisesMutex);
  auto it = rpcPromises.find(req.responds_to());
  if (it == rpcPromises.end()) {
    // no such promise
    std::cerr << "No such promise: " << req.responds_to() << std::endl;
    return;
  }

  auto promise = it->second;
  promise->set_value(req.args());
  rpcPromises.erase(it);
}

} // namespace net
} // namespace celte
