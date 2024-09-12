#include "ClientStatesDeclaration.hpp"

namespace celte {
namespace client {
namespace states {
void Connecting::entry() {
  std::cerr << "Entering StateConnecting" << std::endl;
  // TODO: read uuids here
}

void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

void Connecting::react(EConnectionSuccess const &event) {
  // TODO initialize basic client consumers and producers here
  transit<Connected>();
}
} // namespace states
} // namespace client
} // namespace celte