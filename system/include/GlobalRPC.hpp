#pragma once
#include "CustomRPC.hpp"

namespace celte {
    class Global : public CustomRPCTemplate {
    public:
        Global();
        void RPCHandler(std::string RPCname, std::string args)
        {
            Handler(RPCname, args, tp::global_rpc);
        }
    };

    REGISTER_RPC(Global, RPCHandler)
}
