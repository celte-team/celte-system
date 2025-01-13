#include "Runtime.hpp"

#ifdef __WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
EXPORT void ConnectToCluster() { RUNTIME.ConnectToCluster(); }
EXPORT void ConnectToClusterWithAddress(const std::string &address, int port) {
  RUNTIME.ConnectToCluster(address, port);
}
}