#include "AuthorityTransfer.hpp"
#include "CelteInputSystem.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"
#include "GrapeRegistry.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#ifdef CELTE_SERVER_MODE_ENABLED
#include "MetricsScrapper.hpp"
#endif

#ifdef __WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
/* ------------------- EXPORT RUNTIME TOP LEVEL FUNCTIONS ------------------- */
#pragma region TOP LEVEL BINDINGS

EXPORT void ConnectToCluster() { RUNTIME.ConnectToCluster(); }
EXPORT void ConnectToClusterWithAddress(const std::string &address, int port) {
  RUNTIME.ConnectToCluster(address, port);
}
EXPORT void CelteTick() { RUNTIME.Tick(); }

EXPORT void RegisterGrape(const std::string &grapeId, bool isLocallyOwned,
                          std::function<void()> onReady = nullptr) {
  GRAPES.RegisterGrape(grapeId, isLocallyOwned, onReady);
}

#ifdef CELTE_SERVER_MODE_ENABLED
EXPORT std::string ContainerCreateAndAttach(const std::string &grapeId,
                                            std::function<void()> onReady) {
  return GRAPES.ContainerCreateAndAttach(grapeId, onReady);
}
#endif

EXPORT bool IsGrapeLocallyOwned(const std::string &grapeId) {
  return GRAPES.IsGrapeLocallyOwned(grapeId);
}

EXPORT void RegisterNewEntity(const std::string &id,
                              __attribute__((unused))
                              const std::string &_ownerSN,
                              const std::string &ownerContainerId) {
  ETTREGISTRY.RegisterEntity({
      .id = id,
      .ownerContainerId = ownerContainerId,
  });
}

EXPORT std::string GetNewUUID() { return RUNTIME.GenUUID(); }

EXPORT void ProcessEntityContainerAssignment(const std::string &entityId,
                                             const std::string &toContainerId,
                                             const std::string &payload,
                                             bool ignoreNoMove) {
  celte::AuthorityTransfer::TransferAuthority(entityId, toContainerId, payload,
                                              ignoreNoMove);
}

EXPORT bool SaveEntityPayload(const std::string &eid,
                              const std::string &payload) {
  return ETTREGISTRY.SaveEntityPayload(eid, payload);
}

#ifdef CELTE_SERVER_MODE_ENABLED
EXPORT void ConnectClientToThisNode(const std::string &clientId,
                                    std::function<void()> then) {
  RUNTIME.GetPeerService().ConnectClientToThisNode(clientId, then);
}

EXPORT void SubscribeClientToContainer(const std::string &clientId,
                                       const std::string &containerId,
                                       std::function<void()> then) {
  RUNTIME.GetPeerService().SubscribeClientToContainer(clientId, containerId,
                                                      then);
}

EXPORT void UnsubscribeClientFromContainer(const std::string &clientId,
                                           const std::string &containerId) {
  RUNTIME.GetPeerService().UnsubscribeClientFromContainer(clientId,
                                                          containerId);
}

EXPORT void UpdateSubscriptionStatus(const std::string &ownerOfContainerId,
                                     const std::string &grapeId,
                                     const std::string &containerId,
                                     bool subscribe) {
  GRAPES.SetRemoteGrapeSubscription(ownerOfContainerId, grapeId, containerId,
                                    subscribe);
}

EXPORT void ProxyTakeAuthority(const std::string &grapeId,
                               const std::string &entityId) {
  GRAPES.ProxyTakeAuthority(grapeId, entityId);
}

EXPORT std::optional<void *>
GetOwnedContainerNativeHandle(const std::string &ownerGrapeId,
                              const std::string &containerId) {
  return GRAPES.GetOwnedContainerNativeHandle(ownerGrapeId, containerId);
}

EXPORT void SetOwnedContainerNativeHandle(const std::string &ownerGrapeId,
                                          const std::string &containerId,
                                          void *handle) {
  GRAPES.SetOwnedContainerNativeHandle(ownerGrapeId, containerId, handle);
}

EXPORT std::vector<celte::Entity::ETTNativeHandle>
GetContainerOwnedEntitiesNativeHandles(const std::string &containerId) {
  return celte::ContainerRegistry::GetInstance().GetOwnedEntitiesNativeHandles(
      containerId);
}

EXPORT void RegisterClient(const std::string &clientId) {
  RUNTIME.GetPeerService().GetClientRegistry().RegisterClient(clientId, "", "");
}

EXPORT void ForgetClient(const std::string &clientId) {
  RUNTIME.GetPeerService().GetClientRegistry().ForgetClient(clientId);
}
#else
EXPORT void DisconnectFromServer() { RUNTIME.Disconnect(); }
#endif

EXPORT bool IsEntityRegistered(const std::string &id) {
  return ETTREGISTRY.IsEntityRegistered(id);
}

EXPORT void SetETTNativeHandle(const std::string &id, void *handle) {
  ETTREGISTRY.SetEntityNativeHandle(id, handle);
}

EXPORT std::optional<void *> GetETTNativeHandle(const std::string &id) {
  return ETTREGISTRY.GetEntityNativeHandle(id);
}

EXPORT void ForgetEntityNativeHandle(const std::string &id) {
  ETTREGISTRY.ForgetEntityNativeHandle(id);
}

EXPORT std::string GetETTOwnerContainerId(const std::string &id) {
  return ETTREGISTRY.GetEntityOwnerContainer(id);
}

#ifdef CELTE_SERVER_MODE_ENABLED
EXPORT void SendEntityDeleteOrder(const std::string &id) {
  ETTREGISTRY.SendEntityDeleteOrder(id);
}

EXPORT void RegisterMetric(const std::string &name,
                           std::function<std::string()> getter) {
  celte::METRICS.RegisterMetric(name, getter);
}

EXPORT void MasterInstantiateServerNode(const std::string &payload) {
  RUNTIME.MasterInstantiateServerNode(payload);
}
#endif

EXPORT bool IsEntityLocallyOwned(const std::string &entityId) {
  return ETTREGISTRY.IsEntityLocallyOwned(entityId);
}

#ifdef CELTE_SERVER_MODE_ENABLED
EXPORT void UpdatePropertyState(const std::string &eid, const std::string &key,
                                const std::string &value) {
  GHOSTSYSTEM.UpdatePropertyState(eid, key, value);
}
EXPORT std::string GetAssignedGrapeId() { return RUNTIME.GetAssignedGrape(); }
#endif

EXPORT std::optional<std::string> PollPropertyUpdate(const std::string &eid,
                                                     const std::string &key) {
  return GHOSTSYSTEM.PollPropertyUpdate(eid, key);
}

#pragma endregion
/* ----------------------------- TASK MANAGEMENT ---------------------------- */
#pragma region TASK MANAGEMENT

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

EXPORT std::map<std::string, int> GetLatency() {
  return RUNTIME.GetPeerService().GetLatency();
}

#pragma endregion

#pragma region NAMED_TASKS
/* ------------------------------- NAMED TASKS ------------------------------ */

#ifdef CELTE_SERVER_MODE_ENABLED
/// @brief This function will return a value when this peer owns the grape
/// <grapeId> and a remote server node used the proxy of this grape to ask it to
/// take authority over an entitiy.
/// @param grapeId
/// @return std::optional<std::tuple<std::string, std::string, std::string,
/// std::string>>
/// Values of the tuple are entityId, fromContainerId, payload.
EXPORT std::optional<std::tuple<std::string, std::string, std::string>>
PopAssignmentByProxy(const std::string &grapeId) {
  std::optional<std::tuple<std::string, std::string, std::string>> result =
      GRAPES.PopNamedTaskFromEngine<std::string, std::string, std::string>(
          grapeId, "proxyTakeAuthority");
  return result;
}
#endif
#pragma endregion

/* --------------------------------- RPC API -------------------------------- */
#pragma region RPC API
EXPORT void RegisterGlobalRPC(const std::string &name,
                              std::function<std::string(std::string)> f) {
  RUNTIME.RegisterCustomGlobalRPC(name, f);
}

EXPORT void RegisterGrapeRPC(const std::string &grapeId,
                             const std::string &name,
                             std::function<std::string(std::string)> f) {
  GRAPES.RunWithLock(grapeId, [name, f](celte::Grape &g) {
    // if (!g.rpcService.has_value()) {
    //   throw std::runtime_error(
    //       "Grape has no RPC service, or it has not been initialized yet.");
    // }
    // g.rpcService->Register<std::string>(name, f);
    std::cout << "RegisterGrapeRPC not implemented yet" << std::endl;
  });
}

EXPORT void CallGlobalRPCNoRetVal(const std::string &name,
                                  const std::string &args) {
  RUNTIME.CallScopedRPCNoRetVal(celte::tp::global_rpc, name, args);
}

EXPORT std::string CallGlobalRPC(const std::string &name,
                                 const std::string &args) {
  return RUNTIME.CallScopedRPC(celte::tp::global_rpc, name, args);
}

EXPORT std::string CallScopedRPC(const std::string &scope,
                                 const std::string &name,
                                 const std::string &args) {
  return RUNTIME.CallScopedRPC(scope, name, args);
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
#pragma region HOOKS
#ifdef CELTE_SERVER_MODE_ENABLED // server hooks --------------------------- */
EXPORT void SetOnGetClientInitialGrapeHook(
    std::function<std::string(const std::string &)> f) {
  RUNTIME.Hooks().onGetClientInitialGrape = f;
}

EXPORT void
SetOnAcceptNewClientHook(std::function<void(const std::string &)> f) {
  RUNTIME.Hooks().onAcceptNewClient = f;
}

EXPORT void
SetOnClientRequestDisconnectHook(std::function<void(const std::string &)> f) {
  RUNTIME.Hooks().onClientRequestDisconnect = f;
}

EXPORT void DisconnectClientFromCluster(const std::string &clientId,
                                        const std::string &payload) {
  RUNTIME.ForceDisconnectClient(clientId, payload);
}

EXPORT void SetOnClientNotSeenHook(std::function<void(const std::string &)> f) {
  RUNTIME.Hooks().onClientNotSeen = f;
}

#else // client hooks ------------------------------------------------------ */

#endif // all peers hooks -------------------------------------------------- */

EXPORT void SetOnClientDisconnectHook(
    std::function<void(const std::string &, const std::string &)> f) {
  RUNTIME.Hooks().onClientDisconnect = f;
}

EXPORT void SetOnLoadGrapeHook(std::function<void(std::string, bool)> f) {
  RUNTIME.Hooks().onLoadGrape = f;
}

EXPORT void SetOnConnectionFailedHook(std::function<void()> f) {
  RUNTIME.Hooks().onConnectionFailed = f;
}

EXPORT void SetOnConnectionSuccessHook(std::function<void()> f) {
  RUNTIME.Hooks().onConnectionSuccess = f;
}

EXPORT void SetOnInstantiateEntityHook(
    std::function<void(const std::string &, const std::string &)> f) {
  RUNTIME.Hooks().onInstantiateEntity = f;
}

EXPORT void SetOnRPCTimeoutHook(std::function<void(const std::string &)> f) {
  RUNTIME.Hooks().onRPCTimeout = f;
}

EXPORT void SetOnDeleteEntityHook(
    std::function<void(const std::string &, const std::string &)> f) {
  RUNTIME.Hooks().onDeleteEntity = f;
}

EXPORT void UploadInputData(const std::string &uuid,
                            const std::string &inputName, bool pressed,
                            float x = 0, float y = 0) {
  ETTREGISTRY.UploadInputData(uuid, inputName, pressed, x, y);
}

EXPORT std::optional<const celte::CelteInputSystem::INPUT>
GetInputCircularBuf(const std::string &uuid, const std::string &InputName) {
  return CINPUT.GetInputCircularBuf(uuid, InputName);
}

EXPORT std::string GetUUID() { return RUNTIME.GetUUID(); }

#pragma endregion
}