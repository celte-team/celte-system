#include "CelteServer.hpp"

namespace celte {
namespace server {
void AServer::react(EConnectionSuccess const &event) {
  std::cerr << "Invalid call to EConnectToServer from the current "
               "client state"
            << std::endl;
}

void AServer::react(EDisconnectFromServer const &event) {
  std::cerr << "Invalid call to EDisconnectFromServer from the current "
               "client state"
            << std::endl;
}

void AServer::react(EConnectToCluster const &event) {
  std::cerr << "Invalid call to EConnectToServer from the current "
               "client state"
            << std::endl;
}
} // namespace server
} // namespace celte