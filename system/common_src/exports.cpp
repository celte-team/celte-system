#include "GrapeRegistry.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"

#ifdef __WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
/* ------------------- EXPORT RUNTIME TOP LEVEL FUNCTIONS ------------------- */
#pragma region Runtime

EXPORT void ConnectToCluster() { RUNTIME.ConnectToCluster(); }
EXPORT void ConnectToClusterWithAddress(const std::string &address, int port) {
  RUNTIME.ConnectToCluster(address, port);
}
EXPORT void CelteTick() { RUNTIME.Tick(); }

#pragma endregion
/* ----------------------------- TASK MANAGEMENT ---------------------------- */
#pragma region TaskManagement

EXPORT void PushTaskToSystem(std::function<void()> task) {
  RUNTIME.ScheduleAsyncTask(task);
}

EXPORT void PushIOTaskToSystem(std::function<void()> task) {
  RUNTIME.ScheduleAsyncIOTask(task);
}

EXPORT bool AdvanceTopLevelExecutorTask() {
  auto task = RUNTIME.TopExecutor().PollEngineTask();
  if (task.has_value()) {
    task.value()();
    return true;
  }
  return false;
}

EXPORT bool AdvanceEntityTask(const std::string &id) {
  auto task = ETTREGISTRY.PollEngineTask(id);
  if (task.has_value()) {
    task.value()();
    return true;
  }
  return false;
}

EXPORT bool AdvanceGrapeTask(const std::string &grapeId) {
  auto task = GRAPES.PollEngineTask(grapeId);
  if (task.has_value()) {
    task.value()();
    return true;
  }
  return false;
}

#pragma endregion

/* --------------------------------- RPC API -------------------------------- */

EXPORT void RegisterGlobalRPC(const std::string &name,
                              std::function<std::string(std::string)> f) {
  RUNTIME.RegisterCustomGlobalRPC(name, f);
}

EXPORT void RegisterGrapeRPC(const std::string &grapeId,
                             const std::string &name,
                             std::function<std::string(std::string)> f) {
  GRAPES.RunWithLock(grapeId, [name, f](celte::Grape &g) {
    if (!g.rpcService.has_value()) {
      throw std::runtime_error(
          "Grape has no RPC service, or it has not been initialized yet.");
    }
    g.rpcService->Register<std::string>(name, f);
  });
}

EXPORT void CallGlobalRPCNoRetVal(const std::string &name,
                                  const std::string &args) {
  RUNTIME.CallScopedRPCNoRetVal(celte::tp::global_rpc, name, args);
}

EXPORT [[nodiscard]] char *
CallGlobalRPC(const std::string &name, const std::string &args, size_t *size) {
  std::string result = RUNTIME.CallScopedRPC(celte::tp::global_rpc, name, args);
  *size = result.size();
  return strdup(result.c_str());
}

EXPORT [[nodiscard]] char *CallScopedRPC(const std::string &scope,
                                         const std::string &name,
                                         const std::string &args,
                                         size_t *size) {
  std::string result = RUNTIME.CallScopedRPC(scope, name, args);
  *size = result.size();
  return strdup(result.c_str());
}

EXPORT void CallScopedRPCNoRetVal(const std::string &scope,
                                  const std::string &name,
                                  const std::string &args) {
  RUNTIME.CallScopedRPCNoRetVal(scope, name, args);
}

EXPORT void CallScopedRPCAsync(const std::string &scope,
                               const std::string &name, const std::string &args,
                               std::function<void(std::string)> callback) {
  RUNTIME.CallScopedRPCAsync(scope, name, args, callback);
}

#pragma endregion
/* -------------------------- EXPORT HOOKS SETTERS -------------------------- */
#pragma region Hooks
#ifdef CELTE_SERVER_MODE_ENABLED // server hooks --------------------------- */
EXPORT void SetOnGetClientInitialGrapeHook(
    std::function<std::string(const std::string &)> f) {
  RUNTIME.Hooks().onGetClientInitialGrape = f;
}

#else // client hooks ------------------------------------------------------ */

#endif // all peers hooks -------------------------------------------------- */

EXPORT void SetOnLoadGrapeHook(std::function<void(std::string)> f) {
  RUNTIME.Hooks().onLoadGrape = f;
}

EXPORT void SetOnConnectionFailedHook(std::function<void()> f) {
  RUNTIME.Hooks().onConnectionFailed = f;
}

EXPORT void SetOnConnectionSuccessHook(std::function<void()> f) {
  RUNTIME.Hooks().onConnectionSuccess = f;
}
#pragma endregion
}