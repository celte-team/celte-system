#pragma once

#include "CRPC.hpp"

namespace celte {
    class CustomRPCTemplate {
        // struct CustomFunc {
        //     std::string name;
        //     std::function<std::string(std::string)> func;
        // };

    protected:
        std::map<std::string, std::function<void(std::string)>> _rpcs;

    public:
        void RegisterRPC(const std::string& name, std::function<void(std::string)> func)
        {
            if (_rpcs.find(name) != _rpcs.end()) {
                std::cerr << "RPC " << name << " already registered" << std::endl;
                return;
            }
            _rpcs[name] = func;
        }

        inline void Handler(std::string RPCname, std::string args, std::string id)
        {

            if (_rpcs.find(RPCname) != _rpcs.end())
                _rpcs[RPCname](args);
            else
                std::cerr << "RPC " << RPCname << " not found in instance " << id << " with args " << args << std::endl;
        }
    };
}
