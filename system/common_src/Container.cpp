#include "AuthorityTransfer.hpp"
#include "CelteError.hpp"
#include "Container.hpp"
#include "GrapeRegistry.hpp"
#include "RPCService.hpp"
#include "Topics.hpp"
#include <algorithm>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

using namespace celte;

Container::Container()
    : _id(boost::uuids::to_string(boost::uuids::random_generator()())),
      _rpcService(
          net::RPCService::Options{.thisPeerUuid = RUNTIME.GetUUID(),
                                   .listenOn = {tp::rpc(_id)},
                                   .reponseTopic = tp::peer(RUNTIME.GetUUID()),
                                   .serviceName = tp::rpc(_id)}) {}

void Container::WaitForNetworkReady(std::function<void(bool)> onReady) {
  RUNTIME.ScheduleAsyncTask([this, onReady]() {
    while (!_rpcService.Ready()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    while (not std::all_of(_readerStreams.begin(), _readerStreams.end(),
                           [](auto &rs) { return rs->Ready(); })) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    onReady(true);
  });
}

bool Container::AttachToGrape(const std::string &grapeId) {
  _grapeId = grapeId;
  bool fail = true;
  GRAPES.RunWithLock(grapeId, [this, &fail](Grape &g) {
    fail = false; // if grape not found, this does not happen
    g.containers.insert({_id, this});
    _isLocallyOwned = g.isLocallyOwned;
  });
  if (fail) {
    std::cerr << "Failed to attach container to grape " << grapeId
              << ". Grape not found." << std::endl;
    return false;
  }
  __initRPCs();
  __initStreams();
  return true;
}

void Container::__initRPCs() {
  _rpcService.Register<bool>("__rp_containerTakeAuthority",
                             std::function([this](std::string args) {
                               __rp_containerTakeAuthority(args);
                               return true;
                             }));

  _rpcService.Register<bool>("__rp_containerDropAuthority",
                             std::function([this](std::string args) {
                               __rp_containerDropAuthority(args);
                               return true;
                             }));
}

void Container::__initStreams() {
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_isLocallyOwned) {
    _replws = _createWriterStream<req::ReplicationDataPacket>(
        net::WriterStream::Options{.topic = {tp::repl(_id)}});
  } else {
#endif

    _createReaderStream<req::ReplicationDataPacket>(
        {.thisPeerUuid = RUNTIME.GetUUID(),
         .topics = {tp::peer(RUNTIME.GetUUID())},
         .subscriptionName = tp::peer(RUNTIME.GetUUID()),
         .exclusive = false,
         .messageHandlerSync = [this](const pulsar::Consumer,
                                      req::ReplicationDataPacket req) {},
         .messageHandler =
             [this](const pulsar::Consumer, req::ReplicationDataPacket req) {
               std::cout << "[[Container]] handling replication data packet : "
                         << req.DebugString() << std::endl;
             }});

#ifdef CELTE_SERVER_MODE_ENABLED
  }
#endif
}

void Container::__rp_containerTakeAuthority(const std::string &args) {
  try {
    nlohmann::json j = nlohmann::json::parse(args);
    AuthorityTransfer::ExecTakeOrder(j);
  } catch (const std::exception &e) {
    THROW_ERROR(net::RPCHandlingException, e.what());
  }
}

void Container::__rp_containerDropAuthority(const std::string &args) {
  try {
    nlohmann::json j = nlohmann::json::parse(args);
    AuthorityTransfer::ExecDropOrder(j);
  } catch (const std::exception &e) {
    THROW_ERROR(net::RPCHandlingException, e.what());
  }
}