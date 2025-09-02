#include "Topics.hpp"

namespace celte {
namespace tp {
static std::string default_scope_name =
    "non-persistent://public/default/"; // not const, will be set when choosing
                                        // the session id

std::string rpc(const std::string &str) {
  return default_scope_name + str + ".rpc";
}
std::string peer(const std::string &str) { return default_scope_name + str; }
std::string input(const std::string &str) {
  return default_scope_name + str + ".input";
}
std::string repl(const std::string &str) {
  return default_scope_name + str + ".repl";
}
std::string hello_master_sn() { return default_scope_name + "master.hello.sn"; }
std::string hello_master_cl() {
  return default_scope_name + "master.hello.client";
}
std::string global_clock() { return default_scope_name + "global.clock"; }
std::string global_rpc() { return default_scope_name + "global.rpc"; }
std::string hello_client() { return default_scope_name + "client.hello"; }

std::string &default_scope() { return default_scope_name; }

} // namespace tp
} // namespace celte
