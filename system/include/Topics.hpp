#pragma once
#include <string>
namespace celte {
    namespace tp {
        static const std::string default_scope = "persistent://public/default/";

        inline std::string rpc(const std::string& str)
        {
            return default_scope + str + ".rpc";
        }
        inline std::string peer(const std::string& str) { return default_scope + str; }
        inline std::string input(const std::string& str)
        {
            return default_scope + str + ".input";
        }
        inline std::string repl(const std::string& str)
        {
            return default_scope + str + ".repl";
        }

        static const std::string hello_master_sn = default_scope + "master.hello.sn";
        static const std::string hello_master_cl = default_scope + "master.hello.client";
        static const std::string global_clock = default_scope + "global.clock";
        static const std::string global_rpc = "global";

    } // namespace tp
} // namespace celte
