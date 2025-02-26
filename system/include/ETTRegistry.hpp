#pragma once
#include "Entity.hpp"
#include <exception>
#include <expected>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <tbb/concurrent_hash_map.h>

#define ETTREGISTRY celte::ETTRegistry::GetInstance()

namespace celte {

class ETTAlreadyRegisteredException : public std::exception {
public:
  ETTAlreadyRegisteredException(const std::string &id)
      : _id(id), _msg("Entity with id " + id + " already exists.") {}

  const char *what() const noexcept override { return _msg.c_str(); }

private:
  std::string _id;
  std::string _msg;
};

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

  /// @brief Calls the delete entity hook on all entities owned by the container
  /// whose id is passed in argument.
  /// @param containerId
  void DeleteEntitiesInContainer(const std::string &containerId);

  inline storage &GetEntities() { return _entities; }

  /// @brief Instantiates an entity in the engine by calling the
  /// onInstantiateEntity hook.
  void EngineCallInstantiate(const std::string &ettId,
                             const std::string &payload,
                             const std::string &ownerContainerId);

  /// @brief Runs a function with a lock on the entity.
  /// @param id
  /// @param f
  void RunWithLock(const std::string &id, std::function<void(Entity &)> f) {
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

  /// @brief Returns the id of the container that owns the entity.
  /// @param id
  /// @return
  std::string GetEntityOwnerContainer(const std::string &id);
  void SetEntityOwnerContainer(const std::string &id,
                               const std::string &ownerContainer);

  bool IsEntityLocallyOwned(const std::string &id);

  bool IsEntityQuarantined(const std::string &id);
  void SetEntityQuarantined(const std::string &id, bool quarantine);

  bool IsEntityValid(const std::string &id);
  void SetEntityValid(const std::string &id, bool isValid);

  inline void SetEntityNativeHandle(const std::string &id, void *handle) {
    accessor acc;
    if (_entities.find(acc, id)) {
      acc->second.ettNativeHandle = handle;
    }
  }

  inline std::optional<void *> GetEntityNativeHandle(const std::string &id) {
    accessor acc;
    if (_entities.find(acc, id)) {
      return acc->second.ettNativeHandle;
    }
    return std::nullopt;
  }

  void ForgetEntityNativeHandle(const std::string &id);

  void Clear();

  /// @brief Loads all entities that are already instantiated in this container
  /// in the server node that owns the container. This will call the
  /// onInstantiateEntity hook in the engine.
  void LoadExistingEntities(const std::string &grapeId,
                            const std::string &containerId);
  bool SaveEntityPayload(const std::string &eid, const std::string &payload);

#ifdef CELTE_SERVER_MODE_ENABLED

  /// @brief Called over the network to fetch the entities that exist in a
  /// container in order to load them.
  /// @param containerId
  std::map<std::string, std::string>
  GetExistingEntities(const std::string &containerId);
  std::expected<std::string, std::string>
  GetEntityPayload(const std::string &eid);

  /// @brief If the entity is locally owned, notifies the network that it should
  /// be deleted from the game world. If the entity is not locally owned,
  /// nothing happens.
  /// @param id
  void SendEntityDeleteOrder(const std::string &id);
#endif

  ///@brief Send an input to kafka, this will trigger a RPC in the other client
  /// and server. Define in CelteInputSystem
  ///@param inputName String name/id of the input
  ///@param pressed   Bool   status of the input (true for down false for up)
  void UploadInputData(std::string uuid, std::string inputName, bool pressed,
                       float x = 0, float y = 0);

  /// @brief  Returns true if the entity is registered in the registry.
  /// @param id
  /// @return true if the entity is registered, false otherwise.
  bool IsEntityRegistered(const std::string &id) {
    accessor acc;
    return _entities.find(acc, id);
  }

private:
  storage _entities;
};
} // namespace celte
