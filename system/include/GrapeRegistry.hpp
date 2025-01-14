#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "Executor.hpp"
#include "RPCService.hpp"
#include <string>
#include <tbb/concurrent_hash_map.h>

#define GRAPES celte::GrapeRegistry::GetInstance()

namespace celte {
struct Grape {
  std::string id;      ///< The unique identifier of the grape.
  bool isLocallyOwned; ///< True if this grape is owned by this peer. (only
                       ///< possible in server mode)
#ifdef CELTE_SERVER_MODE_ENABLED
  std::optional<ClientRegistry> clientRegistry; ///< The client registry for
                                                ///< this grape. Only available
                                                ///< in server mode.
#endif
  Executor executor; ///< The executor for this grape. Tasks to be ran in the
                     ///< engine can be pushed here.
  std::optional<net::RPCService>
      rpcService; ///< The rpc service for this grape. Must be initialized after
                  ///< the id has been set.
};

class GrapeRegistry {
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
    }
  }

  template <typename T>
  void RunWithLock(const std::string &grapeId, T *instance,
                   void (T::*memberFunc)(Grape &)) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      (instance->*memberFunc)(acc->second);
    }
  }

  template <typename T, typename... Args>
  void RunWithLock(const std::string &grapeId, T *instance,
                   void (T::*memberFunc)(Grape &, Args...), Args... args) {
    accessor acc;
    if (_grapes.find(acc, grapeId)) {
      (instance->*memberFunc)(acc->second, args...);
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

private:
  tbb::concurrent_hash_map<std::string, Grape> _grapes;
};
} // namespace celte
