#pragma once
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace celte {

/**
 * @brief An entity container is a container that holds entities that should be
 * replicated all together as one unit. Entities can be assigned and removed
 * from the container using their uuids.
 *
 * Users are encouraged to derive their own implementations of this class to
 * implement different replication strategies.
 */
class EntityContainer {
public:
  /**
   * @brief Assign an entity to the container. Its properties will be replicated
   * to this container's channels. If the entity is a client, it's inputs will
   * be replicated to this container's channels. Removing the entity from the
   * previous owner is not this container's responsibility.
   */
  virtual void AssignEntity(const std::string &entityId);

  /**
   * @brief Remove an entity from the container. The entity will no longer be
   * replicated to this container's channels. If the entity is a client, its
   * inputs will no longer be replicated to this container's channels.
   */
  virtual void RemoveEntity(const std::string &entityId);

  /**
   * @brief Merge the entities of this container into the other container.
   * This container will be empty after the merge, ready to be deleted.
   */
  virtual void Merge(std::shared_ptr<EntityContainer> other);

  /**
   * @brief Split the entities of this container into two containers.
   * The entities will be split in a way that the two containers will have
   * approximately the same number of entities.
   */
  virtual std::shared_ptr<EntityContainer> Split();

  /**
   * @brief Returns the features of this container as a JSON object, so that it
   * can be analysed by the replication nodes.
   */
  virtual nlohmann::json GetFeatures();
};

/**
 * @brief Given an entity an a chunk, Assignment Replication Nodes will return a
 * score between zero and one that represents the probability of the entity
 * needing to be owned by this chunk.
 *
 * ARNs exist manage locally owned grapes and chunks. Instances of grapes that
 * are owned by other SNs use ZARNs instead.
 *
 * Users are encouraged to derive their own implementations of this class to
 * implement different replication strategies.
 */
class ARN {
public:
  /**
   * @brief Returns a score between zero and one that represents the probability
   * of the entity needing to be owned by this chunk.
   */
  virtual float GetScore(const std::string &entityId) = 0;
};

/**
 * @brief Interest Replication nodes are used to determine the interest of a
 * locally owned client in a chunk. The IRN either returns yes or no (true or
 * false). Remote SN an advertise their chunks to the local SN and IRNs will
 * assign locally owned clients to the remote chunks.
 *
 * IRNs are owned by instances of grapes that are locally owned.
 *
 * Users are encouraged to derive their own implementations of this class to
 */
class IRN {};

/**
 * @brief Replication Graphs are used to manage the ARNs and IRNs
 * of a grape, as well as to instantiate chunks if needed.
 *
 * Local Entity containers and clients are advertised to remote replication
 * graphs so that they can choose (or not) to replicate them.
 */
class ReplicationGraph {
public:
  void AddARN(std::shared_ptr<ARN> arn);
  void AddIRN(std::shared_ptr<IRN> irn);
  void Sort(std::vector<EntityContainer> &containers,
            std::vector<std::string> &entities);
};
} // namespace celte