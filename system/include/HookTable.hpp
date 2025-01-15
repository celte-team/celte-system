#pragma once
#include <functional>
#include <string>

template <typename Ret, typename... Args>
std::function<Ret(Args...)> UNIMPLEMENTED =
    [](Args...) -> Ret { throw std::runtime_error("Missing hook"); };

namespace celte {
struct HookTable {

#ifdef CELTE_SERVER_MODE_ENABLED // server only hooks

  /// @brief Called by the master when a lcient connects to the cluster, to get
  /// the name of the grape that the client should be connecting to.
  std::function<std::string(const std::string &)> onGetClientInitialGrape =
      UNIMPLEMENTED<std::string, const std::string &>;

  /// @brief Called when a new client is accepted by the server.
  /// The game developer is free to handle this as they see fit. Eventually, the
  /// client should connect to a grape in order to be able to load the map and
  /// start playing.
  /// @param clientId The unique identifier of the client.
  std::function<std::string(const std::string &)> onAcceptNewClient =
      UNIMPLEMENTED<std::string, const std::string &>;

#else // client only hooks

#endif // all peers hooks

  /// @brief Called when a grape is loaded (the game should load the map and the
  /// CSN object associated with it).
  std::function<void(std::string, bool)> onLoadGrape =
      UNIMPLEMENTED<void, std::string, bool>;

  /// @brief Called when the connection to the cluster is unsuccessful.
  std::function<void()> onConnectionFailed = UNIMPLEMENTED<void>;

  /// @brief Called when the connection to the cluster is successful.
  std::function<void()> onConnectionSuccess = UNIMPLEMENTED<void>;

  /// @brief Called when an entity has to be instantiated in the engine.
  std::function<void(std::string)> onInstantiateEntity =
      UNIMPLEMENTED<void, std::string>;

  /* ----------------------------- ERROR HANDLERS -----------------------------
   */

  /// @brief Called when an RPC call times out.
  std::function<void(const std::string &)> onRPCTimeout =
      UNIMPLEMENTED<void, const std::string &>;
};
} // namespace celte