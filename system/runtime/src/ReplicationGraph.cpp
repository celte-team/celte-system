#include "CelteGrape.hpp"
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

std::shared_ptr<IEntityContainer> ReplicationGraph::AddContainer() {
  std::lock_guard<std::mutex> lock(_containersMutex);
  auto container = _instantiateContainer(
      boost::uuids::to_string(boost::uuids::random_generator()()));
  _containers.push_back(container);
  return container;
}

std::shared_ptr<IEntityContainer>
ReplicationGraph::AddContainer(const std::string &id) {
  std::lock_guard<std::mutex> lock(_containersMutex);
  auto container = _instantiateContainer(id);
  _containers.push_back(container);
  return container;
}

#ifdef CELTE_SERVER_MODE_ENABLED
void ReplicationGraph::TakeEntity(const std::string &entityId) {
  if (_containers.empty()) {
    if (_instantiateContainer == nullptr) {
      throw std::runtime_error(
          "No container available and no way to create one");
    }
    AddContainer();
  }

  try {
    std::cout << "in replicator graph's take entity" << std::endl;
    CelteEntity &e = RUNTIME.GetEntityManager().GetEntity(entityId);
    AssignEntityByAffinity(e);
  } catch (std::out_of_range &e) {
    std::cout << "TakeEntity: " << e.what() << std::endl;
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

void ReplicationGraph::RegisterEntityContainer(
    std::shared_ptr<IEntityContainer> container) {
  std::lock_guard<std::mutex> lock(_containersMutex);
  _containers.push_back(container);
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
  std::vector<float> scores;
  scores.reserve(_containers.size());

  for (const auto &container : _containers) {
    scores.push_back(_getAffinity(entity, container));
  }

  auto maxIt = std::max_element(scores.begin(), scores.end());
  if (maxIt != scores.end()) {
    size_t index = std::distance(scores.begin(), maxIt);
    float maxScore = *maxIt;

    if (maxScore < 0.01) {
      return std::nullopt;
    }

    std::shared_ptr<IEntityContainer> bestContainer = _containers[index];
    return ContainerAffinity{.container = bestContainer, .affinity = maxScore};
  }
  return std::nullopt;
}

void ReplicationGraph::AssignEntityByAffinity(CelteEntity &entity) {
  std::optional<ContainerAffinity> bestContainerScore =
      GetBestContainerForEntity(entity);
  if (not bestContainerScore.has_value()) {
    // no container has enough affinity with the entity, look in other grapes
    std::cout
        << "no container has enough affinity with the entity, looking elsewhere"
        << std::endl;
    __lookupBestContainerInOtherGrapes(entity);
    return;
  }
  if (bestContainerScore->container->GetId() != entity.GetContainerId()) {
    bestContainerScore->container->TakeEntity(entity.GetUUID());
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
                             "RegisterOwnerGrapeId");
  }
}

nlohmann::json ReplicationGraph::Dump() const {
  nlohmann::json j;
  j["number of containers"] = _containers.size();
  for (const auto &container : _containers) {
    j["containers"].push_back(std::map<std::string, std::string>{
        {"id", container->GetId()},
        {"locally owned", container->IsLocallyOwned() ? "true" : "false"},
        {"owned entities", std::to_string(container->GetNOwnedEntities())},
    });
  }
  return j;
}

std::optional<std::shared_ptr<IEntityContainer>>
ReplicationGraph::GetContainerOpt(const std::string &id) {
  std::lock_guard<std::mutex> lock(_containersMutex);
  for (auto &container : _containers) {
    if (container->GetId() == id) {
      return container;
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
