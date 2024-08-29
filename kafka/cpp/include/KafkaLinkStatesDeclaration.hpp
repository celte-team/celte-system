/*
** CELTE, 2024
** statemachine
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** ClientFSM.hpp
*/

#pragma once
#include "KafkaFSM.hpp"
#include <string>

namespace celte {
    namespace nl {
        namespace states {

            // ==========================================================================
            // States
            // ==========================================================================

            /**
             * @brief Kafka did not connect to a cluster yet.
             *
             * To connect, dispatch the EConnectToCluster event.
             */
            class KLDisconnected : public AKafkaLink {

                void entry() override;
                void exit() override;

                void react(EConnectToCluster const& event) override;
            };

            /**
             * @brief Kafka Link will fall in this state if the connection to
             * the cluster could not be established or if the master's entry
             * topic could not be found.
             */
            class KLErrorCouldNotConnect : public AKafkaLink {
                void entry() override;
                void exit() override;
            };

            /** @brief Kafka Link is connected to the kafka cluster and is ready
             * to handle the creation of consumers and producers.
             *
             * A thread is launched to automatically poll from all registered
             * consumers and schedule callbacks for execution.
             */
            class KLConnected : public AKafkaLink {
                void entry() override;
                void exit() override;
            };

        } // namespace states
    } // namespace client
} // namespace celte
