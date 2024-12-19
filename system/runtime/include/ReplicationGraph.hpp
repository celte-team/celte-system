#pragma once
#include "RPCService.hpp"
#include "nlohmann/json.hpp"
#include "queue.hpp"
#include "topics.hpp"
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace celte {
class CelteEntity;

/**
 * @brief A container for a collection of entities.
 * All entites in this container will be replicated as a group.
 *
 */
class IEntityContainer : public net::CelteService {
public:
  IEntityContainer(const std::string &_combinedId);
  virtual ~IEntityContainer() = default;

  /**
   * @brief Returns a reference to the Grape that owns this container.
   */
  virtual const std::string &GetGrapeId() const = 0;

  /**
   * @brief Returns the features of this container.
   * Features describe the container and can be used for debugging or by
   * other containers / nodes whishing to anaylize the replication graph.
   */
  virtual nlohmann::json GetFeatures() const = 0;

  /**
   * @brief Initializes the container. This method should be called only once.
   *
   * @return std::string the id that can be used to reference the network
   * channels of this container.
   */
  virtual std::string Initialize() = 0;

  /**
   * @brief This method should return true if the container is owned by the
   * current peer. (will always return false on client side).
   */
  virtual bool IsLocallyOwned() const = 0;

  /**
   * @brief Waits for the network to be initialized.
   */
  virtual void WaitNetworkInitialized() = 0;

  inline net::RPCService &GetRPCService() { return _rpcs; }

  virtual std::string GetRPCTopicId() const {
    return tp::PERSIST_DEFAULT + _id + "." + tp::RPCs;
  }

  virtual std::string GetReplTopicId() const {
    return tp::PERSIST_DEFAULT + _id + "." + tp::REPLICATION;
  }

  virtual std::string GetInputTopicId() const {
    return tp::PERSIST_DEFAULT + _id + "." + tp::INPUT;
  }

  virtual std::string GetId() const { return _id; }

  /**
   * @brief This method is called to assign an entity to this container, in
   * response to an order from the rightful owner of the entity.
   */
  virtual void TakeEntityLocally(const std::string &entityId) = 0;

  inline void IncNOwnedEntities(int n) { _nOwnedEntities += n; }

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Takes ownership of an entity.
   */
  virtual void TakeEntity(const std::string &entityId) = 0;

  virtual void SpawnEntityOnNetwork(const std::string &entityId, float x,
                                    float y, float z) = 0;

#endif

  unsigned int GetNOwnedEntities() const { return _nOwnedEntities; }

protected:
  std::string _id;
  net::RPCService _rpcs;
  unsigned int _nOwnedEntities = 0;

#ifdef CELTE_SERVER_MODE_ENABLED
  std::shared_ptr<net::WriterStream> _replicationWS;
  std::unordered_map<std::string, std::string> _nextScheduledReplicationData;
  std::unordered_map<std::string, std::string>
      _nextScheduledActiveReplicationData;
#endif
};

class ReplicationGraph {
public:
  using AssignmentReplNode = std::function<float(
      CelteEntity &entity, std::shared_ptr<IEntityContainer> container)>;

  ReplicationGraph();
  virtual ~ReplicationGraph();

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Registers an entity in the replication graph and assigns it to a
   * container. If no container exists or none is suitable for the entity, a new
   * one is created. This method will effectively drop the ownership of the
   * entity. This method should be called when the entity enters the grape
   * (which is technically decided by a Server interest replication node).
   */
  void TakeEntity(const std::string &entityId);
#endif

  /**
   * @brief Manually registers a new container in the replication graph so that
   * it can be take into account when assigning entities.
   */
  void RegisterEntityContainer(std::shared_ptr<IEntityContainer> container);

  inline void AddContainer() {
    std::lock_guard<std::mutex> lock(_containersMutex);
    _containers.push_back(_instantiateContainer());
  }

  /**
   * @brief Returns a json object containing data about the replication graph,
   * its containers and the replicated entities.
   */
  nlohmann::json Dump() const;

  /**
   * @brief Registers the Id of the owner grape so that directives can be
   * broadcasted to the right channel.
   */
  void RegisterOwnerGrapeId(const std::string &grapeId);

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Sets the logic used to reassign nodes to containers.
   */
  void SetAssignmentReplNode(AssignmentReplNode arn) { _getAffinity = arn; }

  struct ContainerAffinity {
    std::shared_ptr<IEntityContainer> container;
    float affinity;
  };

  std::optional<ContainerAffinity>
  GetBestContainerForEntity(CelteEntity &entity);

  void __lookupBestContainerInOtherGrapes(CelteEntity &entity);

  /**
   * @brief Given a container and an entity, this method will assign the entity
   * to the container (without checking if it is the best container for the
   * entity and without broadcasting the change to the network). The entity will
   * be added to the list of entities managed by the graph. This method should
   * be called when the entity spawns in this grape.
   */
  void TakeEntityLocally(const std::string &entityId,
                         std::shared_ptr<IEntityContainer> container);
#endif

  inline void SetInstantiateContainer(
      std::function<std::shared_ptr<IEntityContainer>()> instantiateContainer) {
    _instantiateContainer = instantiateContainer;
  }

  /**
   * @brief Validates that the hooks and information necessary for this
   * replication graph have been setup correctly.
   *
   * @throws std::runtime_error if the graph is not valid.
   */
  void Validate();

  inline std::shared_ptr<IEntityContainer> // TODO: store containers in an
                                           // unordered map to avoid the loop
  GetContainerById(const std::string &id) {
    std::lock_guard<std::mutex> lock(_containersMutex);
    for (auto &container : _containers) {
      if (container->GetId() == id) {
        return container;
      }
    }
    return nullptr;
  }

  /**
   * @brief Chooses the best container for a single entity and assigns it to
   * that container.
   */
  void AssignEntityByAffinity(CelteEntity &entity);

protected:
#ifdef CELTE_SERVER_MODE_ENABLED
  AssignmentReplNode _getAffinity = nullptr;
#endif

  std::string _ownerGrapeId;

  std::vector<std::shared_ptr<IEntityContainer>> _containers;
  std::mutex _containersMutex;

  std::function<std::shared_ptr<IEntityContainer>()> _instantiateContainer;
};

} // namespace celte