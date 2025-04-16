#pragma once
#include "CustomRPC.hpp"
#include "Executor.hpp"
#include <string>

namespace celte {
    struct Entity : public CustomRPCTemplate {
        using ETTNativeHandle = void*; ///< The entity's native handle used to
        ///< interact with the engine.

        ~Entity();

        std::string id = ""; ///< The entity's unique identifier.
        std::string ownerContainerId = ""; ///< The entity's owner's container identifier.
        bool quarantine = false; ///< Whether the entity is quarantined (excluded from
                                 ///< network reassignment).
        bool isValid = true; ///< Whether the entity is valid. (if not, it will be
                             ///< ignored by the runtime until it is valid again)
        Executor
            executor; ///< The entity's executor used to push tasks for the entity.

        std::optional<ETTNativeHandle>
            ettNativeHandle; ///< The entity's native handle used to
                             ///< interact with the engine.

        std::string payload; ///< customdata that can be set by the game dev to help
        ///< other peers loading the entity

    public:
        void RPCHandler(std::string RPCname, std::string args)
        {
            Handler(RPCname, args, id);
        }
        void initRPCService();
    };

    REGISTER_RPC(Entity, RPCHandler)

} // namespace celte
