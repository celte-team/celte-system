#pragma once
#include "tinyfsm.hpp"
#include <memory>
#include <string>

namespace celte {
    namespace nl {

        struct EConnectToCluster : tinyfsm::Event {
            std::string ip;
            int port;
            std::shared_ptr<std::string> message;
        };

    }
}