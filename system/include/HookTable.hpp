#pragma once
#include <functional>
#include <iostream>
#include <string>

// HookInvoker stores a heap-allocated std::function and forwards calls/assigns
// to it. The underlying std::function is intentionally leaked to avoid
// destructor-order crashes during shared-library unload / static teardown.
template <typename Sig> struct HookInvoker;

template <typename Ret, typename... Args> struct HookInvoker<Ret(Args...)> {
  using Fn = std::function<Ret(Args...)>;

  HookInvoker() {
    func = new Fn(
        [](Args...) -> Ret { throw std::runtime_error("Missing hook"); });
  }
  explicit HookInvoker(Fn f) { func = new Fn(std::move(f)); }

  // forward call
  Ret operator()(Args... args) const {
    return (*func)(std::forward<Args>(args)...);
  }

  // assign new callable into the existing heap object
  HookInvoker &operator=(const Fn &f) {
    *func = f;
    return *this;
  }
  HookInvoker &operator=(Fn &&f) {
    *func = std::move(f);
    return *this;
  }

  // allow taking the stored function by value if needed
  Fn as_function() const { return *func; }

  // pointer to heap-allocated std::function (intentionally not deleted). Use
  // exported API function CelteCleanup to delete all hooks at once when
  // unloading the shared.
  Fn *func;
};

template <typename Ret, typename... Args>
inline std::function<Ret(Args...)> make_unimplemented() {
  return std::function<Ret(Args...)>(
      [](Args...) -> Ret { throw std::runtime_error("Missing hook"); });
}

namespace celte {
struct HookTable {
#ifdef CELTE_SERVER_MODE_ENABLED // server only hooks

  HookInvoker<void(const std::string &)> onServerReceivedInitializationPayload;

  /// @brief Called by the master when a lcient connects to the cluster, to get
  /// the name of the grape that the client should be connecting to.
  HookInvoker<std::string(const std::string &)> onGetClientInitialGrape;

  /// @brief Called when a new client is accepted by the server.
  /// The game developer is free to handle this as they see fit. Eventually, the
  /// client should connect to a grape in order to be able to load the map and
  /// start playing.
  /// @param clientId The unique identifier of the client.
  /// @param spawnerId The unique identifier of the grape that spawned the
  HookInvoker<void(const std::string &, const std::string &)> onAcceptNewClient;

  HookInvoker<void(const std::string &)> onClientRequestDisconnect;

  HookInvoker<void(const std::string &)> onClientNotSeen;

#else  // client only hooks
#endif // all peers hooks

  /// @brief Called when a client disconnects from the cluster.
  /// @param clientId The unique identifier of the client.
  HookInvoker<void(const std::string &, const std::string &)>
      onClientDisconnect;

  /// @brief Called when a grape is loaded (the game should load the map and the
  /// CSN object associated with it).
  HookInvoker<void(const std::string &, bool)> onLoadGrape;

  /// @brief Called when the connection to the cluster is unsuccessful.
  HookInvoker<void()> onConnectionFailed;

  /// @brief Called when the connection to the cluster is successful.
  HookInvoker<void()> onConnectionSuccess;

  /// @brief Called when an entity has to be instantiated in the engine.
  HookInvoker<void(const std::string &, const std::string &)>
      onInstantiateEntity;

  /// @brief Called when an entity has to be deleted in the engine.
  /// @param entityId The unique identifier of the entity.
  /// @param payload The payload of the entity.
  HookInvoker<void(const std::string &, const std::string &)> onDeleteEntity;

  /* ----------------------------- ERROR HANDLERS -----------------------------
   */

  /// @brief Called when an RPC call times out.
  HookInvoker<void(const std::string &)> onRPCTimeout =
      HookInvoker<void(const std::string &)>([](const std::string &s) {
        std::cerr << "RPC call timed out: " << s << std::endl;
      });

  HookInvoker<void(const std::string &)> onRPCHandlingError =
      HookInvoker<void(const std::string &)>([](const std::string &s) {
        std::cerr << "Error handling RPC: " << s << std::endl;
      });
};
} // namespace celte