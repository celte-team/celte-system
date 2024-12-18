#include "CelteRuntime.hpp"
#include "ReplicationGraph.hpp"
#include <algorithm>

namespace celte {
IEntityContainer::IEntityContainer(const std::string &id)
    : _rpcs({
          .thisPeerUuid = RUNTIME.GetUUID(),
          .listenOn = {tp::PERSIST_DEFAULT + id + "." + celte::tp::RPCs},
          .serviceName = RUNTIME.GetUUID() + ".chunk." + id + "." + tp::RPCs,
      }),
      _id(id) {}

#ifdef CELTE_SERVER_MODE_ENABLED
void ReplicationGraph::TakeEntity(const std::string &entityId) {
  if (_containers.empty()) {
    if (_instantiateContainer == nullptr) {
      throw std::runtime_error(
          "No container available and no way to create one");
    }
    {
      std::lock_guard<std::mutex> lock(*_containersMutex);
      _containers.push_back(_instantiateContainer());
    }
  }

  try {
    CelteEntity &e = RUNTIME.GetEntityManager().GetEntity(entityId);
    __assignEntityByAffinity(e);
  } catch (std::out_of_range &e) {
    std::cout << "TakeEntity: " << e.what() << std::endl;
  }

  std::lock_guard<std::mutex> lock(*_entitiesMutex);
  _entities.push_back(entityId);
}
#endif

ReplicationGraph::~ReplicationGraph() {
#ifdef CELTE_SERVER_MODE_ENABLED
  _reassignThreadRunning = false;
  if (_reassignThread.joinable()) {
    _reassignThread.join();
  }
#endif
}

void ReplicationGraph::RegisterEntityContainer(
    std::shared_ptr<IEntityContainer> container) {
  std::lock_guard<std::mutex> lock(*_containersMutex);
  _containers.push_back(container);
}

ReplicationGraph::ReplicationGraph()
    : _entitiesMutex(new std::mutex()), _containersMutex(new std::mutex) {}

void ReplicationGraph::SetOwnerGrapeId(const std::string &grapeId) {
  _ownerGrapeId = grapeId;
}

#ifdef CELTE_SERVER_MODE_ENABLED
void ReplicationGraph::__reassignEntities() {
  if (_entities.empty() or _containers.empty()) {
    return;
  }
  // avoid locking for too long by copying the entities
  std::vector<std::string> entities;
  {
    std::lock_guard<std::mutex> lock(*_entitiesMutex);
    entities = _entities;
  }

  for (auto &entity : entities) {
    // find the best container for the entity
    try {
      CelteEntity &e = RUNTIME.GetEntityManager().GetEntity(entity);
      __assignEntityByAffinity(e);
    } catch (std::out_of_range &e) {
      std::cout << "__reassign Entities: " << e.what() << std::endl;
    }
  }
}

void ReplicationGraph::__assignEntityByAffinity(CelteEntity &entity) {
  std::shared_ptr<IEntityContainer> bestContainer = nullptr;
  decltype(_containers) containers;
  {
    std::lock_guard<std::mutex> lock(*_containersMutex);
    containers = _containers;
  }

  auto bestIt = std::max_element(
      containers.begin(), containers.end(),
      [&](const std::shared_ptr<IEntityContainer> &a,
          const std::shared_ptr<IEntityContainer> &b) {
        return _getAffinity(entity, a) < _getAffinity(entity, b);
      });

  if (bestIt != containers.end()) {
    bestContainer = *bestIt;
    if (bestContainer->GetId() != entity.GetContainerId()) {
      bestContainer->TakeEntity(entity.GetUUID());
    }
  } else {
    throw std::runtime_error("No container available to assign entity to.");
  }
}
#endif

void ReplicationGraph::Validate() {
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_getAffinity == nullptr) {
    throw std::runtime_error("AssignmentReplNode not set. Set it using "
                             "SetAssignmentReplNode");
  }
#endif
  if (_instantiateContainer == nullptr) {
    throw std::runtime_error("InstantiateContainer not set. Set it using "
                             "SetInstantiateContainer");
  }
  if (_ownerGrapeId.empty()) {
    throw std::runtime_error("OwnerGrapeId not set. Set it using "
                             "SetOwnerGrapeId");
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  _reassignThreadRunning = true;
  _reassignThread = std::thread([this]() {
    while (_reassignThreadRunning) {
      __reassignThreadWorker();
    }
  });
#endif
}

#ifdef CELTE_SERVER_MODE_ENABLED
void ReplicationGraph::__reassignThreadWorker() {
  __reassignEntities();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
#endif

} // namespace celte