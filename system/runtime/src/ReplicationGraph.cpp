#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "ReplicationGraph.hpp"
#include <algorithm>

namespace celte {
IEntityContainer::IEntityContainer(const std::string &id)
    : _rpcs(net::RPCService::Options{
          .thisPeerUuid = RUNTIME.GetUUID(),
          .listenOn = {tp::PERSIST_DEFAULT + id + "." + celte::tp::RPCs},
          .responseTopic = RUNTIME.GetUUID() + "." + tp::RPCs,
          .serviceName = RUNTIME.GetUUID() + ".chunk." + id + "." + tp::RPCs,
      }),
      _id(id) {}

std::shared_ptr<IEntityContainer> ReplicationGraph::AddContainer() {
  auto container = _instantiateContainer(
      boost::uuids::to_string(boost::uuids::random_generator()()));
  // _containers.push_back(container);
  RegisterEntityContainer(container);
  return container;
}

std::shared_ptr<IEntityContainer>
ReplicationGraph::AddContainer(const std::string &id) {
  std::lock_guard<std::mutex> lock(_containersMutex);
  auto containerOpt = GetContainerOpt(id, false);
  if (containerOpt.has_value()) { // container has already been created
    return containerOpt.value();
  }
  auto container = _instantiateContainer(id);
  // _containers.push_back(container);
  RegisterEntityContainer(container, false);
  return container;
}

#ifdef CELTE_SERVER_MODE_ENABLED
void ReplicationGraph::TakeEntity(const std::string &entityId) {
  std::cout << "[[TAKE ENTITY]]" << entityId << std::endl;
  if (_containers.empty()) {
    if (_instantiateContainer == nullptr) {
      throw std::runtime_error(
          "No container available and no way to create one");
    }
    std::cout << "before add container" << std::endl;
    AddContainer();
    std::cout << "after add container" << std::endl;
  }

  try {
    std::cout << "before get entity" << std::endl;
    CelteEntity &e = RUNTIME.GetEntityManager().GetEntity(entityId);
    std::cout << "before assign entity by affinity" << std::endl;
    AssignEntityByAffinity(e);
    std::cout << "after assign entity by affinity" << std::endl;
  } catch (std::out_of_range &e) {
    std::cout << "Entity " << entityId
              << " not found not instantiated on this server node" << std::endl;
  }
}

void ReplicationGraph::TakeEntityLocally(
    const std::string &entityId, std::shared_ptr<IEntityContainer> container) {
  if (container == nullptr) {
    throw std::runtime_error("Container is null");
  }
  if (not container->IsLocallyOwned()) {
    throw std::runtime_error("Container is not locally owned");
  }
  container->TakeEntityLocally(entityId);
}

#endif

ReplicationGraph::~ReplicationGraph() {}

std::shared_ptr<IEntityContainer>
ReplicationGraph::GetContainerById(const std::string &id) {
  std::scoped_lock lock(_containersMutex);
  auto it = _containers.find(id);
  if (it != _containers.end()) {
    return it->second;
  }
  throw std::out_of_range("No container with id " + id + " exists.");
}

void ReplicationGraph::RegisterEntityContainer(
    std::shared_ptr<IEntityContainer> container, bool protect) {
  if (protect) {
    std::lock_guard<std::mutex> lock(_containersMutex);
    _containers.insert({container->GetId(), container});
    return;
  }
  _containers.insert({container->GetId(), container});
}

ReplicationGraph::ReplicationGraph() {}

void ReplicationGraph::RegisterOwnerGrapeId(const std::string &grapeId) {
  _ownerGrapeId = grapeId;
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::optional<ReplicationGraph::ContainerAffinity>
ReplicationGraph::GetBestContainerForEntity(CelteEntity &entity) {
  if (_containers.empty()) {
    return std::nullopt;
  }

  float bestScore = 0;
  std::shared_ptr<IEntityContainer> bestContainer = nullptr;

  for (const auto &[_, container] : _containers) {
    float score = _getAffinity(entity, container);
    if (score > bestScore) {
      bestScore = score;
      bestContainer = container;
    }
  }

  if (bestContainer == nullptr) {
    return std::nullopt;
  }

  return ContainerAffinity{.container = bestContainer, .affinity = bestScore};
}

void ReplicationGraph::AssignEntityByAffinity(CelteEntity &entity) {
  if (ENTITIES.IsQuaranteened(entity.GetUUID())) {
    return;
  }
  std::optional<ContainerAffinity> bestContainerScore =
      GetBestContainerForEntity(entity);
  if (not bestContainerScore.has_value()) {
    // no container has enough affinity with the entity, look in other grapes
    __assignEntityToRemoteGrape(entity);
    return;
  }
  if (bestContainerScore->container->GetId() != entity.GetContainerId()) {
    bestContainerScore->container->TakeEntity(entity.GetUUID());
  }
}

void ReplicationGraph::__assignEntityToRemoteGrape(CelteEntity &entity) {
  if (ENTITIES.IsQuaranteened(entity.GetUUID())) {
    return;
  }
  try {
    std::vector<void *> grapeEngineWrapperPtrs;
    for (auto &grape : GRAPES.GetGrapes()) {
      if (grape->GetGrapeId() == _ownerGrapeId) {
        continue;
      }
      grapeEngineWrapperPtrs.push_back(grape->GetEngineWrapperInstancePtr());
    }
    // the node that loads the entity should ideally already be aware of the
    // existence of the entity had have it loaded in game
    std::string newOwnerId = _sarn(entity.GetWrapper(), grapeEngineWrapperPtrs);
    auto &newOwnerGrape = GRAPES.GetGrape(newOwnerId);
    // remove entity from assignment logic to avoid double assignment
    ENTITIES.QuaranteenEntity(entity.GetUUID());
    // remote takes entity
    newOwnerGrape.RemoteTakeEntity(entity.GetUUID());
  } catch (std::out_of_range &e) {
    std::cerr << "Error in __assignEntityToRemoteGrape: " << e.what()
              << std::endl;
  }
}

void ReplicationGraph::__lookupBestContainerInOtherGrapes(CelteEntity &entity) {
  std::optional<ContainerAffinity> bestContainerScore;
  for (auto &grape : GRAPES.GetGrapes()) {
    if (grape->GetGrapeId() == _ownerGrapeId) {
      continue;
    }
    std::optional<ContainerAffinity> score =
        grape->GetReplicationGraph().GetBestContainerForEntity(entity);
    if (not score.has_value()) {
      continue;
    }
    if (not bestContainerScore.has_value() or
        score->affinity > bestContainerScore->affinity) {
      bestContainerScore = score;
    }
  }
  if (not bestContainerScore.has_value()) {
    std::cerr << "No container or grape known to this server node "
                 "has enough affinity with the entity"
              << std::endl;
    return;
  }
  bestContainerScore->container->TakeEntity(entity.GetUUID());
}

#endif

void ReplicationGraph::UpdateRemoteContainer(const std::string &cid,
                                             nlohmann::json info) {
  bool isRelevant = _irn(info);
  if (isRelevant) {
    __loadRemoteContainer(cid, info);
  } else {
    __removeRemoteContainer(cid);
  }
}

void ReplicationGraph::__loadRemoteContainer(const std::string &cid,
                                             nlohmann::json info) {
  if (GetContainerOpt(cid).has_value()) {
    return;
  }
  std::shared_ptr<IEntityContainer> container = AddContainer(cid);
  container->WaitNetworkInitialized();
  container->Load(info["features"]);
  container->LoadExistingEntities();
}

void ReplicationGraph::__removeRemoteContainer(const std::string &cid) {
  auto container = GetContainerOpt(cid);
  if (container.has_value()) {
    container.value()->Remove();
    std::lock_guard<std::mutex> lock(_containersMutex);
    std::cout << "REPL GRAPH " << this << " removing container " << cid
              << std::endl;
    _containers.erase(cid);
  }
}

void ReplicationGraph::Validate() {
#ifdef CELTE_SERVER_MODE_ENABLED
  if (_getAffinity == nullptr) {
    throw std::runtime_error("AssignmentReplNode not set. Set it using "
                             "SetAssignmentReplNode");
    if (_sarn == nullptr) {
      throw std::runtime_error("ServerAssignmentReplNode not set. Set it using "
                               "SetServerAssignmentReplNode");
    }
  }
#endif

  if (_irn == nullptr) {
    throw std::runtime_error("InterestReplNode not set. Set it using "
                             "SetInterestReplicationNode");
  }
  if (_instantiateContainer == nullptr) {
    throw std::runtime_error("InstantiateContainer not set. Set it using "
                             "SetInstantiateContainer");
  }
  if (_ownerGrapeId.empty()) {
    throw std::runtime_error("OwnerGrapeId not set. Set it using "
                             "RegisterOwnerGrapeId");
  }
}

nlohmann::json ReplicationGraph::Dump() const {
  nlohmann::json j;
  j["number of containers"] = _containers.size();
  for (const auto &[id, container] : _containers) {
    j["containers"].push_back(std::map<std::string, std::string>{
        {"id", id},
        {"locally owned", container->IsLocallyOwned() ? "true" : "false"},
        {"owned entities", std::to_string(container->GetNOwnedEntities())},
    });
  }
  return j;
}

std::optional<std::shared_ptr<IEntityContainer>>
ReplicationGraph::GetContainerOpt(const std::string &id, bool block) {
  if (block) {
    std::lock_guard<std::mutex> lock(_containersMutex);
    auto it = _containers.find(id);
    if (it != _containers.end()) {
      return it->second;
    }
  } else {
    auto it = _containers.find(id);
    if (it != _containers.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

unsigned int ReplicationGraph::GetNumberOfContainers() const {
  auto &ownerGrape = GRAPES.GetGrape(_ownerGrapeId);
  if (ownerGrape.GetOptions().isLocallyOwned) {
    return _containers.size();
  } else {
    // not calling on rpc channel to only target the owner node
    return ownerGrape.GetRPCService().Call<unsigned int>(
        tp::PERSIST_DEFAULT + _ownerGrapeId, "GetNumberOfContainers");
  }
}

} // namespace celte
