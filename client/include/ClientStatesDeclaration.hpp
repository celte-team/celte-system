#pragma once
#include "CelteClient.hpp"
#include "ClientEvents.hpp"
#include "tinyfsm.hpp"

namespace celte {
    namespace client {
        namespace states {
            class Disconnected : public AClient {
                void entry() override;
                void exit() override;

                void react(celte::nl::EConnectToCluster const& event) override;
            };

            class Connecting : public AClient {
                void entry() override;
                void exit() override;

                void react(EConnectionSuccess const& event) override;
            };

            class Connected : public AClient {
                void entry() override;
                void exit() override;

                void react(EDisconnectFromServer const& event) override;
            };
        } // namespace states
    } // namespace client
} // namespace celte