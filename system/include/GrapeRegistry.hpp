#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "Executor.hpp"
#include "Grape.hpp"
#include "RPCService.hpp"
#include <string>
#include <tbb/concurrent_hash_map.h>

#define GRAPES celte::GrapeRegistry::GetInstance()

namespace celte {
class GrapeRegistry : net::CelteService {
public:
  using accessor = tbb::concurrent_hash_map<std::string, Grape>::accessor;
  static GrapeRegistry &GetInstance();

  /// @brief Registers a grape for this server node. The grape is either locally
  /// owned or not. If it is not locally owned, it won't have a client registry.
  /// @param grapeId
  /// @param isLocallyOwned
  /// @param onReady
  void RegisterGrape(const std::string &grapeId, bool isLocallyOwned,
                     std::function<void()> onReady = nullptr);
  void UnregisterGrape(const std::string &grapeId);

  inline tbb::concurrent_hash_map<std::string, Grape> &GetGrapes() {
    return _grapes;
  }

  /// @brief Checks if a container exists in the grape registry.
  bool ContainerExists(const std::string &containerId);

  /// @brief Returns true if the grape with the given id exists in the registry.
  bool GrapeExists(const std::string &grapeId);

  /// @brief Pushes a task that will be asynchronously executed by the system.
  /// If the task is an I/O task, use PushIOTaskToSystem instead.
  /// @param grapeId
  /// @param task
  inline void PushTaskToSystem(const std::string &grapeId,
                               std::function<void()> task) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      acc->second.executor.PushTaskToSystem(task);
    }
  }

  /// @brief Pushes a task that will be asynchronously executed by the system.
  /// This task is optimized for I/O tasks.
  inline void PushIOTaskToSystem(const std::string &grapeId,
                                 std::function<void()> task) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      acc->second.executor.PushIOTaskToSystem(task);
    }
  }

  /// @brief Pushes a task that will be executed in the engine's context.
  inline void PushTaskToEngine(const std::string &grapeId,
                               std::function<void()> task) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      acc->second.executor.PushTaskToEngine(task);
    }
  }

  /// @brief  Returns the next task in the engine queue, if any. Use this method
  /// only if you are in the engine's context.
  /// @param grapeId
  /// @return
  inline std::optional<std::function<void()>>
  PollEngineTask(const std::string &grapeId) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      return acc->second.executor.PollEngineTask();
    }
    return std::nullopt;
  }

  inline void RunWithLock(const std::string &grapeId,
                          std::function<void(Grape &)> f) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      f(acc->second);
    } else {
      std::cout << "Grape not found: " << grapeId << std::endl;
    }
  }

  template <typename T>
  void RunWithLock(const std::string &grapeId, T *instance,
                   void (T::*memberFunc)(Grape &)) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      (instance->*memberFunc)(acc->second);
    } else {
      std::cout << "Grape not found: " << grapeId << std::endl;
    }
  }

  template <typename T, typename... Args>
  void RunWithLock(const std::string &grapeId, T *instance,
                   void (T::*memberFunc)(Grape &, Args...), Args... args) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      (instance->*memberFunc)(acc->second, args...);
    } else {
      std::cout << "Grape not found: " << grapeId << std::endl;
    }
  }

  inline std::string GetGrapeId(const std::string &grapeId) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      return acc->second.id;
    }
    return "";
  }

  inline bool IsGrapeLocallyOwned(const std::string &grapeId) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      return acc->second.isLocallyOwned;
    }
    return false;
  }

  inline void SetGrapeLocallyOwned(const std::string &grapeId,
                                   bool isLocallyOwned) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      acc->second.isLocallyOwned = isLocallyOwned;
    }
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Creates a container and attaches it to the grape. The container
  /// will wait for its network service to be ready before calling the onReady
  /// callback.
  /// @param grapeId
  /// @param onReady
  /// @return The id of the created container.
  std::string ContainerCreateAndAttach(std::string grapeId,
                                       std::function<void()> onReady,
                                       const std::string &id = "");

  void SetRemoteGrapeSubscription(const std::string &grapeId,
                                  const std::string &containerId,
                                  bool subscribe);

  bool SubscribeGrapeToContainer(const std::string &grapeId,
                                 const std::string &containerId,
                                 std::function<void()> onReady);
  void UnsubscribeGrapeFromContainer(const std::string &grapeId,
                                     const std::string &containerId);
#endif

  /// @brief If a server node detects that an entity should be owned by a
  /// non locally owned grape, it can notify the grape through its proxy so that
  /// the remote grape can take authority over the entity.
  void ProxyTakeAuthority(const std::string &grapeId,
                          const std::string &entityId,
                          const std::string &fromContainerId,
                          const std::string &payload);

private:
  tbb::concurrent_hash_map<std::string, Grape> _grapes;
};
} // namespace celte
