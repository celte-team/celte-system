#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"
#include "nlohmann/json.hpp"

namespace celte {
namespace client {
namespace states {
void Connected::entry() { __registerRPCs(); }

void Connected::exit() { __unregisterRPCs(); }

void Connected::react(EDisconnectFromServer const &event) {
  logs::Logger::getInstance().err() << "Disconnecting from server" << std::endl;
  transit<Disconnected>();
}

void Connected::__registerRPCs() {}

void Connected::__unregisterRPCs() {}

} // namespace states
} // namespace client
} // namespace celte
