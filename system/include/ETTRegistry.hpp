#pragma once
#include "Entity.hpp"
#include <expected>
#include <map>
#include <string>
#include <tbb/concurrent_hash_map.h>

#define ETTREGISTRY celte::ETTRegistry::GetInstance()

namespace celte {

class ETTRegistry {
public:
  using storage = tbb::concurrent_hash_map<std::string, Entity>;
  using accessor = storage::accessor;
  static ETTRegistry &GetInstance();

  /// @brief Registers an entity in the registry.
  /// @param e The entity to register.
  void RegisterEntity(const Entity &e);

  /// @brief Unregisters an entity from the registry.
  /// @param id The entity's unique identifier.
  void UnregisterEntity(const std::string &id);

  /// @brief Pushes a task to the entity's executor. The task will be ran in the
  /// entity's engine loop.
  void PushTaskToEngine(const std::string &id, std::function<void()> task);

  /// @brief Polls the next task in the entity's engine tasks. Use only from the
  /// context of the engine.
  std::optional<std::function<void()>> PollEngineTask(const std::string &id);

  inline storage &GetEntities() { return _entities; }

  /// @brief Instantiates an entity in the engine by calling the
  /// onInstantiateEntity hook.
  void EngineCallInstantiate(const std::string &ettId,
                             const std::string &payload,
                             const std::string &ownerContainerId);

  /// @brief Runs a function with a lock on the entity.
  /// @param id
  /// @param f
  inline void RunWithLock(const std::string &id,
                          std::function<void(Entity &)> f) {
    accessor acc;
    if (_entities.find(acc, id)) {
      f(acc->second);
    }
  }

  /// @brief Runs a member function with a lock on the entity.
  /// @param id
  /// @param instance
  template <typename T>
  void RunWithLock(const std::string &id, T *instance,
                   void (T::*memberFunc)(Entity &)) {
    accessor acc;
    if (_entities.find(acc, id)) {
      (instance->*memberFunc)(acc->second);
    }
  }

  template <typename T, typename... Args>
  void RunWithLock(const std::string &id, T *instance,
                   void (T::*memberFunc)(Entity &, Args...), Args... args) {
    accessor acc;
    if (_entities.find(acc, id)) {
      (instance->*memberFunc)(acc->second, args...);
    }
  }

  /// @brief Returns the id of the server node that owns the entity.
  /// @param id
  /// @return
  std::string_view GetEntityOwner(const std::string &id);
  void SetEntityOwner(const std::string &id, const std::string &owner);

  /// @brief Returns the id of the container that owns the entity.
  /// @param id
  /// @return
  std::string_view GetEntityOwnerContainer(const std::string &id);
  void SetEntityOwnerContainer(const std::string &id,
                               const std::string &ownerContainer);

  bool IsEntityQuarantined(const std::string &id);
  void SetEntityQuarantined(const std::string &id, bool quarantine);

  bool IsEntityValid(const std::string &id);
  void SetEntityValid(const std::string &id, bool isValid);

  void Clear();

  /// @brief Loads all entities that are already instantiated in this container
  /// in the server node that owns the container. This will call the
  /// onInstantiateEntity hook in the engine.
  void LoadExistingEntities(const std::string &grapeId,
                            const std::string &containerId);

#ifdef CELTE_SERVER_MODE_ENABLED

  /// @brief Called over the network to fetch the entities that exist in a
  /// container in order to load them.
  /// @param containerId
  std::map<std::string, std::string>
  GetExistingEntities(const std::string &containerId);
  bool SaveEntityPayload(const std::string &eid, const std::string &payload);
  std::expected<std::string, std::string>
  GetEntityPayload(const std::string &eid);
#endif

private:
  storage _entities;
};
} // namespace celte
