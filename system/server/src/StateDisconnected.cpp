#include "ServerStatesDeclaration.hpp"

namespace celte {
namespace server {
namespace states {
void Disconnected::entry() {
  std::cerr << "Server is Disconnected" << std::endl;
}

void Disconnected::exit() {}

void Disconnected::react(EConnectToCluster const &event) {
  transit<Connecting>();
}
} // namespace states
} // namespace server
} // namespace celte