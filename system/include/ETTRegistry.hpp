#pragma once
#include "Entity.hpp"
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

  std::string_view GetEntityOwner(const std::string &id);
  void SetEntityOwner(const std::string &id, const std::string &owner);

  std::string_view GetEntityOwnerContainer(const std::string &id);
  void SetEntityOwnerContainer(const std::string &id,
                               const std::string &ownerContainer);

  bool IsEntityQuarantined(const std::string &id);
  void SetEntityQuarantined(const std::string &id, bool quarantine);

  bool IsEntityValid(const std::string &id);
  void SetEntityValid(const std::string &id, bool isValid);

  void Clear();

private:
  storage _entities;
};
} // namespace celte
