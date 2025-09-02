#include "GlobalRPC.hpp"

using namespace celte;

Global::Global() { GlobalRPCHandlerReactor::subscribe(tp::global_rpc(), this); }
