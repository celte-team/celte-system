/*
** EPITECH PROJECT, 2025
** celte-system
** File description:
** Container
*/

#include "AuthorityTransfer.hpp"
#include "CelteError.hpp"
#include "CelteInputSystem.hpp"
#include "Container.hpp"
#include "GrapeRegistry.hpp"
#include "Logger.hpp"

#include "Topics.hpp"
#include <algorithm>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>

using namespace celte;

class ContainerCreationException : public CelteError {
public:
  ContainerCreationException(const std::string &msg, Logger &log,
                             std::string file, int line)
      : CelteError(msg, log, file, line) {}
};

Container::Container()
    : _id(boost::uuids::to_string(boost::uuids::random_generator()())) {}

void Container::WaitForNetworkReady(std::function<void()> onReady) {
  RUNTIME.ScheduleAsyncTask([this, onReady]() {
    while (not std::all_of(_readerStreams.begin(), _readerStreams.end(),
                           [](auto &rs) { return rs->Ready(); })) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    onReady();
  });
}

Container::~Container() {
  ContainerTakeAuthorityReactor::unsubscribe(tp::rpc(_id));
  ContainerDropAuthorityReactor::unsubscribe(tp::rpc(_id));
  ContainerDeleteEntityReactor::unsubscribe(tp::rpc(_id));
}

void Container::__initRPCs() {
  ContainerTakeAuthorityReactor::subscribe(tp::rpc(_id), this);
  ContainerDropAuthorityReactor::subscribe(tp::rpc(_id), this);
  ContainerDeleteEntityReactor::subscribe(tp::rpc(_id), this);
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
         .topics = {tp::repl(_id)},
         .subscriptionName = tp::peer(RUNTIME.GetUUID()),
         .exclusive = false,
         .messageHandlerSync = [this](const pulsar::Consumer,
                                      req::ReplicationDataPacket req) {},
         .messageHandler =
             [this](const pulsar::Consumer, req::ReplicationDataPacket req) {
               GhostSystem::HandleReplicationPacket(req);
             }});

#ifdef CELTE_SERVER_MODE_ENABLED
  }
#endif

  _createReaderStream<req::InputUpdate>({
      .thisPeerUuid = RUNTIME.GetUUID(),
      .topics = {tp::input(_id)},
      .subscriptionName = tp::peer(RUNTIME.GetUUID()),
      .exclusive = false,
      .messageHandlerSync =
          [this](const pulsar::Consumer, req::InputUpdate req) {
            CINPUT.HandleInput(req.uuid(), req.name(), req.pressed(), req.x(),
                               req.y());
          },
  });
}

void Container::TakeAuthority(std::string args) {
  try {
    nlohmann::json j = nlohmann::json::parse(args);
    AuthorityTransfer::ExecTakeOrder(j);
  } catch (const std::exception &e) {
    THROW_ERROR(AuthorityTransferException, e.what());
  }
}

void Container::DropAuthority(std::string args) {
  try {
    nlohmann::json j = nlohmann::json::parse(args);
    AuthorityTransfer::ExecDropOrder(j);
  } catch (const std::exception &e) {
    THROW_ERROR(AuthorityTransferException, e.what());
  }
}

void Container::DeleteEntity(std::string entityId, std::string payload) {
  RUNTIME.TopExecutor().PushTaskToEngine([entityId, payload]() {
    ETTREGISTRY.SetEntityValid(entityId, false);
    RUNTIME.Hooks().onDeleteEntity(entityId, payload);
    ETTREGISTRY.UnregisterEntity(entityId);
  });
}

/* --------------------------- CONTAINER REGISTRY --------------------------- */

ContainerRegistry &ContainerRegistry::GetInstance() {
  static ContainerRegistry instance;
  return instance;
}

void ContainerRegistry::RunWithLock(const std::string &containerId,
                                    std::function<void(ContainerRefCell &)> f) {
  accessor acc;
  if (_containers.find(acc, containerId)) {
    f(acc->second);
  } else {
    std::cerr << "Lock on container failed, container was not found: "
              << containerId << std::endl;
  }
}

std::string ContainerRegistry::CreateContainerIfNotExists(const std::string &id,
                                                          bool *wasCreated) {
  accessor acc;
  if (_containers.find(acc, id)) {
    *wasCreated = false;
    return acc->second.id;
  }

  // generating a new ID if the provided ID is empty
  std::string containerId =
      id.empty() ? boost::uuids::to_string(boost::uuids::random_generator()())
                 : id;

  // Emplace the new ContainerRefCell into the hash map
  bool ok = _containers.emplace(acc, containerId, containerId);
  if (!ok) {
    THROW_ERROR(ContainerCreationException,
                "Failed to create container " + containerId);
  }
  *wasCreated = true;
  return containerId;
}

void ContainerRegistry::UpdateRefCount(const std::string &containerId) {
  accessor acc;
  if (_containers.find(acc, containerId)) {
    if (acc->second.container.use_count() == 1) {
      ETTREGISTRY.DeleteEntitiesInContainer(containerId);
      std::cout << "trashing container " << containerId.substr(0, 4)
                << std::endl;
      RUNTIME.GetTrashBin().TrashItem(std::move(acc->second.GetContainerPtr()));
      _containers.erase(acc);
      LOGDEBUG("Container " + containerId +
               " is no longer referenced and has been deleted.");
    }
  }
}
