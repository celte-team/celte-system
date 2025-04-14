#pragma once
#include "CustomRPC.hpp"
#include "Executor.hpp"
#include <boost/circular_buffer.hpp>
#include <map>
#include <memory>
#include <string>

namespace celte {

    typedef struct DataInput_s {
        bool status;
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        float x;
        float y;
    } DataInput_t;

    typedef std::map<std::string, boost::circular_buffer<DataInput_t>> LIST_INPUT;
    typedef boost::circular_buffer<DataInput_t> INPUT;

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

        std::shared_ptr<LIST_INPUT> inputs = std::make_shared<LIST_INPUT>();

    public:
        void RPCHandler(std::string RPCname, std::string args)
        {
            Handler(RPCname, args, id);
        }
        void initRPCService();
    };

    REGISTER_RPC(Entity, RPCHandler)

} // namespace celte
