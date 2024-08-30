#pragma once

#include "tinyfsm.hpp"
#include <memory>
#include <string>

namespace celte {
    namespace server {

        /**
         * @brief Event to signal that the network services have successfully
         * connected to the server. Upon receiving this message, the client will
         * switch to the Connected state.
         */
        struct EConnectionSuccess : tinyfsm::Event {
            std::string ip;
            int port;
            std::shared_ptr<std::string> message;
        };

        /**
         * @brief Event to signal that the network services have disconnected
         * from the server. Upon receiving this message, the client will switch
         * to the Disconnected state.
         */
        struct EDisconnectFromServer : tinyfsm::Event {
            std::shared_ptr<std::string> message;
        };
    }
}