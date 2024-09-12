#include "ClientStatesDeclaration.hpp"

namespace celte {
namespace client {
namespace states {
void Disconnected::entry() {
  std::cerr << "Client is Disconnected" << std::endl;
}

void Disconnected::exit() {}

void Disconnected::react(EConnectToCluster const &event) {
  transit<Connecting>();
}
} // namespace states
} // namespace client
} // namespace celte