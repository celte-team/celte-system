#pragma once
#include "RPCService.hpp"
#include "nlohmann/json.hpp"
#include "queue.hpp"
#include "topics.hpp"
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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
   * @brief Loads the features of this container from a json object. This is
   * meant to init the container to a state that is described by the features,
   * to mirror the state of a remote container.
   */
  virtual void Load(const nlohmann::json &features) = 0;

  /**
   * @brief Returns a reference to the Grape that owns this container.
   */
  virtual const std::string &GetGrapeId() const = 0;

  virtual nlohmann::json GetConfigJSON() const = 0;

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Returns the features of this container.
   * Features describe the container and can be used for debugging or by
   * other containers / nodes whishing to anaylize the replication graph.
   */
  virtual nlohmann::json GetFeatures() = 0;
#endif

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
   * @brief The container removes all its entities and free all resources.
   * Data in the container won't be replicated anymore.
   */
  virtual void Remove() = 0;

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

  virtual void SetId(const std::string &id) { _id = id; }

  /**
   * @brief This method is called to assign an entity to this container, in
   * response to an order from the rightful owner of the entity.
   */
  virtual void TakeEntityLocally(const std::string &entityId) = 0;

  inline void IncNOwnedEntities(int n) { _nOwnedEntities += n; }

  virtual void LoadExistingEntities() = 0;

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Takes ownership of an entity.
   */
  virtual void TakeEntity(const std::string &entityId) = 0;

  // virtual void SpawnEntityOnNetwork(const std::string &entityId, float x,
  //                                   float y, float z) = 0;

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
  static constexpr float DEFAULT_AFFINITY_SCORE = 0.5f;

  /**
   * @brief ARN is used to decide if an entity should be transferred to another
   * grape.
   */
  using AssignmentReplNode = std::function<float(
      CelteEntity &entity, std::shared_ptr<IEntityContainer> container)>;

  /**
   * @brief SARN is used to decide to which grape an entity should be
   * transferred when all local containers return a low affinity score.
   */
  using ServerAssignmentReplNode =
      std::function<std::string(void *, std::vector<void *>)>;

  /**
   * @brief IRN is used to decide if a remote container is relevant to this
   * peer (should we subscribe to it?).
   */
  using InterestReplNode = std::function<bool(nlohmann::json)>;

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
  void RegisterEntityContainer(std::shared_ptr<IEntityContainer> container,
                               bool lock = true);

  std::shared_ptr<IEntityContainer> AddContainer();
  std::shared_ptr<IEntityContainer> AddContainer(const std::string &id);

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
  inline void SetAssignmentReplNode(AssignmentReplNode arn) {
    _getAffinity = arn;
  }

  inline void SetServerAssignmentReplNode(ServerAssignmentReplNode sarn) {
    _sarn = sarn;
  }

  struct ContainerAffinity {
    std::shared_ptr<IEntityContainer> container;
    float affinity;
  };

  std::optional<ContainerAffinity>
  GetBestContainerForEntity(CelteEntity &entity);

  void __lookupBestContainerInOtherGrapes(CelteEntity &entity);
  void __assignEntityToRemoteGrape(CelteEntity &entity);

  /**
   * @brief Given a container and an entity, this method will assign the
   * entity to the container (without checking if it is the best container
   * for the entity and without broadcasting the change to the network). The
   * entity will be added to the list of entities managed by the graph. This
   * method should be called when the entity spawns in this grape.
   */
  void TakeEntityLocally(const std::string &entityId,
                         std::shared_ptr<IEntityContainer> container);

#endif

  inline void SetInterestReplNode(InterestReplNode irn) { _irn = irn; }

  /**
   * @brief Updates a remote container : decides if the container is relevant to
   * this grape (i.e should it be instantiated and its entities replicated
   * here).
   */
  void UpdateRemoteContainer(const std::string &cid, nlohmann::json info);

  inline void SetInstantiateContainer(
      std::function<std::shared_ptr<IEntityContainer>(const std::string &)>
          instantiateContainer) {
    _instantiateContainer = instantiateContainer;
  }

  /**
   * @brief Validates that the hooks and information necessary for this
   * replication graph have been setup correctly.
   *
   * @throws std::runtime_error if the graph is not valid.
   */
  void Validate();

  std::shared_ptr<IEntityContainer> // TODO: store containers in an
                                    // unordered map to avoid the loop
  GetContainerById(const std::string &id);

  std::optional<std::shared_ptr<IEntityContainer>>
  GetContainerOpt(const std::string &containerId, bool block = true);

  /**
   * @brief Chooses the best container for a single entity and assigns it to
   * that container.
   */
  void AssignEntityByAffinity(CelteEntity &entity);

  unsigned int GetNumberOfContainers() const;

  inline std::unordered_map<std::string, std::shared_ptr<IEntityContainer>> &
  GetContainers() {
    return _containers;
  }

protected:
#ifdef CELTE_SERVER_MODE_ENABLED
  AssignmentReplNode _getAffinity = nullptr;
  ServerAssignmentReplNode _sarn = nullptr;
#endif
  InterestReplNode _irn = nullptr;

  void __loadRemoteContainer(const std::string &cid, nlohmann::json info);
  void __removeRemoteContainer(const std::string &cid);

  std::string _ownerGrapeId;

  // std::vector<std::shared_ptr<IEntityContainer>> _containers;
  std::unordered_map<std::string, std::shared_ptr<IEntityContainer>>
      _containers;
  std::mutex _containersMutex;

  std::function<std::shared_ptr<IEntityContainer>(const std::string &)>
      _instantiateContainer = nullptr;
};

} // namespace celte