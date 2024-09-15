#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"

namespace celte {
namespace server {
namespace states {
void Connecting::entry() {
  std::cerr << "Entering StateConnecting" << std::endl;
  auto &kfk = RUNTIME.KPool();
  // kfk.Subscribe(
  // "UUID",
  // [this](const kafka::clients::consumer::ConsumerRecord &record) {
  //   try {
  //     std::string uuid(static_cast<const char *>(record.value().data()),
  //                      record.value().size());
  //     std::cerr << "Received UUID: " << uuid << std::endl;
  //     RUNTIME.SetUUID(uuid);

  //     // this will transit all services to Connected
  //     dispatch(EConnectionSuccess());
  //   } catch (const std::exception &e) {
  //     std::cerr << "Error in Connecting::entry: " << e.what() << std::endl;
  //     transit<Disconnected>();
  //   }
  // },
  // false, "UUID");
}

void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

void Connecting::react(EConnectionSuccess const &event) {
  // TODO initialize basic client consumers and producers here
  transit<Connected>();
}
} // namespace states
} // namespace server
} // namespace celte