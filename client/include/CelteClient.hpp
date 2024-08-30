#pragma once
#include "ClientEvents.hpp"
#include "KafkaEvents.hpp"
#include "tinyfsm.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace celte {
    namespace client {
        class AClient : public tinyfsm::Fsm<AClient> {
            friend class Fsm;

        public:
            // ==========================================================================
            // Event reactions
            // ==========================================================================
            inline virtual void react(tinyfsm::Event const&)
            {
                std::cerr << "Unhandled client fsm event" << std::endl;
            };

            virtual void react(EConnectionSuccess const& event);
            virtual void react(EDisconnectFromServer const& event);
            virtual void react(celte::nl::EConnectToCluster const& event);

            // ==========================================================================
            // Entry points
            // ==========================================================================
            virtual void entry(void) = 0;
            virtual void exit(void) = 0;
        };
    } // namespace client
} // namespace celte