#pragma once
#include <string>

namespace celte {
namespace tp {

std::string rpc(const std::string &str);
std::string peer(const std::string &str);
std::string input(const std::string &str);
std::string repl(const std::string &str);
std::string hello_master_sn();
std::string hello_master_cl();
std::string global_clock();
std::string global_rpc();
std::string hello_client();
std::string &default_scope();

} // namespace tp
} // namespace celte
