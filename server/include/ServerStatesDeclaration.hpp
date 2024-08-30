#pragma once
#include "CelteServer.hpp"
#include "ServerEvents.hpp"
#include "tinyfsm.hpp"

namespace celte {
    namespace server {
        namespace states {
            class Disconnected : public AServer {
                void entry() override;
                void exit() override;

                void react(celte::nl::EConnectToCluster const& event) override;
            };

            class Connecting : public AServer {
                void entry() override;
                void exit() override;

                void react(EConnectionSuccess const& event) override;
            };

            class Connected : public AServer {
                void entry() override;
                void exit() override;

                void react(EDisconnectFromServer const& event) override;
            };
        } // namespace states
    } // namespace server
} // namespace celte