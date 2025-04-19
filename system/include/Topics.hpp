#pragma once
#include <string>

namespace celte {
namespace tp {
static std::string default_scope =
    "persistent://public/default/"; // not const, will be set when choosing the
                                    // session id

inline std::string rpc(const std::string &str) {
  return default_scope + str + ".rpc";
}
inline std::string peer(const std::string &str) { return default_scope + str; }
inline std::string input(const std::string &str) {
  return default_scope + str + ".input";
}
inline std::string repl(const std::string &str) {
  return default_scope + str + ".repl";
}

static std::string hello_master_sn() {
  return default_scope + "master.hello.sn";
}

static std::string hello_master_cl() {
  return default_scope + "master.hello.client";
}
static std::string global_clock() { return default_scope + "global.clock"; }
static std::string global_rpc() { return default_scope + "global"; }
static std::string hello_client() { return default_scope + "client.hello"; }

} // namespace tp
} // namespace celte
