#include "Runtime.hpp"

#ifdef __WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
/* ------------------- EXPORT RUNTIME TOP LEVEL FUNCTIONS ------------------- */

EXPORT void ConnectToCluster() { RUNTIME.ConnectToCluster(); }
EXPORT void ConnectToClusterWithAddress(const std::string &address, int port) {
  RUNTIME.ConnectToCluster(address, port);
}
EXPORT void CelteTick() { RUNTIME.Tick(); }

/* -------------------------- EXPORT HOOKS SETTERS -------------------------- */

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
}