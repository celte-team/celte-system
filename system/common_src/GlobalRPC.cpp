#include "GlobalRPC.hpp"

using namespace celte;

Global::Global() {
  GlobalRPCHandlerReactor::subscribe(tp::rpc(tp::global_rpc()), this);
}
