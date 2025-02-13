#pragma once
#include <string>

namespace celte {
    namespace tp {
        static const std::string MASTER_HELLO_CLIENT = "master.hello.client";
        static const std::string MASTER_HELLO_SN = "master.hello.sn";
        static const std::string HEADER_PEER_UUID = "peer.uuid";
        static const std::string RPCs = "rpc";
        static const std::string GLOBAL = "global";
        static const std::string ENTER = "en";
        static const std::string EXIT = "ex";
        static const std::string SPAWN = "sp";
        static const std::string DESPAWN = "dp";
        static const std::string AUTHORITY = "a";
        static const std::string TAKE = "t";
        static const std::string DROP = "d";
        static const std::string GHOSTS = "g";
        static const std::string HEADER_RPC_UUID = "rpcUUID";
        static const std::string answer = "answer";
        static const std::string GLOBAL_CLOCK = "global.clock";
        static const std::string REPLICATION = "repl";
        static const std::string PERSIST_DEFAULT =
            // "non-persistent://public/default/";
            "persistent://public/default/";
        static const std::string INPUT = "input";
    } // namespace tp
} // namespace celte
