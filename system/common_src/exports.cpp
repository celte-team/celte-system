#include "AuthorityTransfer.hpp"
#include "CelteInputSystem.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"
#include "Grape.hpp"
#include "GrapeRegistry.hpp"
#include "HttpClient.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"
#include <exception>
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

#ifdef CELTE_SERVER_MODE_ENABLED
EXPORT void ServerNodeConnect() { RUNTIME.Connect(); }
#else
EXPORT void ClientConnect(const std::string &celteHost, int port,
                          const std::string &sessionId) {
  RUNTIME.Connect(celteHost, port, sessionId);
}
#endif

EXPORT void
SendHttpRequest(const std::string &url, const std::string &jsonBody,
                std::function<void(const std::string &)> callback,
                std::function<void(int, const std::string &)> errorCallback) {
  RUNTIME.ScheduleAsyncIOTask([callback, errorCallback, url, jsonBody]() {
    try { // declare http client here to avoid RAII :)
      celte::HttpClient http(errorCallback);
      auto response = http.Post(url, nlohmann::json::parse(jsonBody));
      callback(response);
    } catch (std::exception &e) {
      errorCallback(500, e.what());
    }
  });
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

EXPORT void CallCMIInstantiate(const std::string &grapeId,
                               const std::string &cmiId,
                               const std::string &prefabId,
                               const std::string &payload,
                               const std::string &clientId) {
  celte::CallGrapeCMIInstantiate()
      .on_peer(grapeId)
      .on_fail_log_error()
      .fire_and_forget(cmiId, prefabId, payload, clientId);
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

EXPORT std::optional<
    std::tuple<std::string, std::string, std::string, std::string>>
PopCMIInstantiate(const std::string &grapeId) {
  std::optional<std::tuple<std::string, std::string, std::string, std::string>>
      result =
          GRAPES.PopNamedTaskFromEngine<std::string, std::string, std::string,
                                        std::string>(grapeId, "CMIInstantiate");
  return result;
}

#endif
#pragma endregion

/* --------------------------------- RPC API -------------------------------- */
#pragma region RPC API
EXPORT void RegisterGlobalRPC(const std::string &name, int filter,
                              std::function<void(std::string)> f) {

  if (filter == 0)
    RUNTIME.GetPeerService().GetGlobalRPC().RegisterRPC(name, f);

#ifdef CELTE_SERVER_MODE_ENABLED
  else if (filter == 1)
    RUNTIME.GetPeerService().GetGlobalRPC().RegisterRPC(name, f);

#else
  else if (filter >= 2)
    RUNTIME.GetPeerService().GetGlobalRPC().RegisterRPC(name, f);
#endif
}

EXPORT void RegisterGrapeRPC(const std::string &grapeId, int filter,
                             const std::string &name,
                             std::function<void(std::string)> f) {

  if (filter == 0)
    GRAPES.RunWithLock(grapeId,
                       [name, f](celte::Grape &g) { g.RegisterRPC(name, f); });
#ifdef CELTE_SERVER_MODE_ENABLED
  else if (filter == 1)
    GRAPES.RunWithLock(grapeId,
                       [name, f](celte::Grape &g) { g.RegisterRPC(name, f); });
#else
  else if (filter >= 2)
    GRAPES.RunWithLock(grapeId,
                       [name, f](celte::Grape &g) { g.RegisterRPC(name, f); });
#endif
}

EXPORT void RegisterEntityRPC(const std::string &entityId, int filter,
                              const std::string &name,
                              std::function<void(std::string)> f) {
  if (filter == 0)
    ETTREGISTRY.RunWithLock(
        entityId, [name, f](celte::Entity &e) { e.RegisterRPC(name, f); });
#ifdef CELTE_SERVER_MODE_ENABLED
  else if (filter == 1)
    ETTREGISTRY.RunWithLock(
        entityId, [name, f](celte::Entity &e) { e.RegisterRPC(name, f); });
#else
  else if (filter >= 2)
    ETTREGISTRY.RunWithLock(
        entityId, [name, f](celte::Entity &e) { e.RegisterRPC(name, f); });
#endif
}

EXPORT void RegisterClientRPC(const std::string &clientId, int filter,
                              const std::string &name,
                              std::function<void(std::string)> f) {
#ifdef CELTE_CLIENT_MODE_ENABLED
  if (RUNTIME.GetUUID() == clientId)
    RUNTIME.GetPeerService().RegisterRPC(name, f);
  else
    std::cout << "    NOT GOOD UUID    ";
#endif
}

EXPORT void CallGlobalRPC(const std::string &name, const std::string &args) {
  celte::CallGlobalRPCHandler()
      .on_scope(celte::tp::global_rpc)
      .on_fail_log_error()
      .fire_and_forget(name, args);
}

EXPORT void CallGrapeRPC(bool isPrivate, const std::string &grapeId,
                         const std::string &name, const std::string &args) {
  if (GRAPES.GrapeExists(grapeId))
    if (isPrivate)
      celte::CallGrapeRPCHandler()
          .on_peer(grapeId)
          .on_fail_log_error()
          .fire_and_forget(name, args);
    else
      celte::CallGrapeRPCHandler()
          .on_scope(grapeId)
          .on_fail_log_error()
          .fire_and_forget(name, args);
  else
    std::cout << "Grape not registered" << std::endl;
}

EXPORT void CallEntityRPC(bool isPrivate, const std::string &entityId,
                          const std::string &name, const std::string &args) {
  if (ETTREGISTRY.IsEntityRegistered(entityId) &&
      ETTREGISTRY.IsEntityLocallyOwned(entityId))
    if (isPrivate)
      celte::CallEntityRPCHandler()
          .on_peer(entityId)
          .on_fail_log_error()
          .fire_and_forget(name, args);
    else
      celte::CallEntityRPCHandler()
          .on_scope(entityId)
          .on_fail_log_error()
          .fire_and_forget(name, args);
  else
    std::cout << "Entity not registered" << std::endl;
}

EXPORT void CallClientRPC(const std::string &clientId, const std::string &name,
                          const std::string &args) {

#ifdef CELTE_SERVER_MODE_ENABLED
  celte::CallPeerServiceRPCHandler()
      .on_peer(clientId)
      .on_fail_log_error()
      .fire_and_forget(name, args);
#endif
}

#pragma endregion
/* -------------------------- EXPORT HOOKS SETTERS -------------------------- */
#pragma region HOOKS
#ifdef CELTE_SERVER_MODE_ENABLED // server hooks --------------------------- */
EXPORT void SetOnServerReceivedInitializationPayloadHook(
    std::function<void(const std::string &)> f) {
  RUNTIME.Hooks().onServerReceivedInitializationPayload = f;
}

EXPORT void SetOnGetClientInitialGrapeHook(
    std::function<std::string(const std::string &)> f) {
  RUNTIME.Hooks().onGetClientInitialGrape = f;
}

EXPORT void SetOnAcceptNewClientHook(
    std::function<void(const std::string &, const std::string &)> f) {
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
  std::cout << "before calling runtime" << std::endl;
  RUNTIME.Hooks().onInstantiateEntity = f;
  std::cout << "after calling runtime" << std::endl;
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
