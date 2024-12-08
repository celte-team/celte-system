#include "CelteClient.hpp"
#include "Logger.hpp"

namespace celte {
namespace client {
void AClient::react(EConnectionSuccess const &event) {
  logs::Logger::getInstance().err()
      << "Invalid call to EConnectToServer from the current "
         "client state"
      << std::endl;
}

void AClient::react(EDisconnectFromServer const &event) {
  logs::Logger::getInstance().err()
      << "Invalid call to EDisconnectFromServer from the current "
         "client state"
      << std::endl;
}

void AClient::react(EConnectToCluster const &event) {
  logs::Logger::getInstance().err()
      << "Invalid call to EConnectToServer from the current "
         "client state"
      << std::endl;
}
} // namespace client
} // namespace celte