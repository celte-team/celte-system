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

  /// @brief Returns the id of the grape that the container is attached to, or
  /// std::nullopt if the container is not attached to any grape.
  std::optional<std::string>
  GetOwnerOfContainer(const std::string &containerId);

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
    std::cout << "Running with lock" << std::endl;
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
    std::cout << "Running with lock" << std::endl;
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
    std::cout << "Running with lock" << std::endl;
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

  /// @brief Creates a container and attaches it to the grape. The container
  /// will wait for its network service to be ready before calling the onReady
  /// callback.
  /// @param grapeId
  /// @param onReady
  /// @return The id of the created container.
  std::string ContainerCreateAndAttach(std::string grapeId,
                                       std::function<void()> onReady);

private:
  tbb::concurrent_hash_map<std::string, Grape> _grapes;
};
} // namespace celte
