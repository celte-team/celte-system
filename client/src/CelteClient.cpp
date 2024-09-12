#include "CelteClient.hpp"

namespace celte {
namespace client {
void AClient::react(EConnectionSuccess const &event) {
  std::cerr << "Invalid call to EConnectToServer from the current "
               "client state"
            << std::endl;
}

void AClient::react(EDisconnectFromServer const &event) {
  std::cerr << "Invalid call to EDisconnectFromServer from the current "
               "client state"
            << std::endl;
}

void AClient::react(EConnectToCluster const &event) {
  std::cerr << "Invalid call to EConnectToServer from the current "
               "client state"
            << std::endl;
}
} // namespace client
} // namespace celte