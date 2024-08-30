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
#include "KafkaEvents.hpp"
#include "kafka/KafkaConsumer.h"
#include "tinyfsm.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

namespace celte {
    namespace nl {
        /**
         * @brief base state for all possible states of a connection to a kafka
         * cluster.
         *
         */
        class AKafkaLink : public tinyfsm::Fsm<AKafkaLink> {
            friend class Fsm;
            using ScheduledKCTask
                = std::function<void(kafka::clients::consumer::ConsumerRecord)>;
            using CustomPropsUpdater
                = std::function<void(kafka::Properties& props)>;

        private:
            struct Consumer {
                kafka::clients::consumer::KafkaConsumer consumer;
                std::queue<
                    std::vector<kafka::clients::consumer::ConsumerRecord>>
                    recvdData;
                ScheduledKCTask dataHandler;
            };

        public:
            static std::unordered_map<std::string, Consumer> _consumers;

            static kafka::Properties kDefaultProps;

            /**
             * @brief Creates a new consumer, which will continuously poll for
             * update in its topic. If a message is received, the callback
             * argument will be registered for synchronous execution later on.
             * To execute all registered callbacks, use the Catchback() static
             * method;
             */
            static void RegisterConsumer(const std::string& topic,
                ScheduledKCTask callback,
                CustomPropsUpdater customPropsUpdater = nullptr);

            /**
             * Executes all tasks scheduled from the received messages.
             */
            static void Catchback();

            // ==========================================================================
            // Event reactions
            // ==========================================================================

            /**
             * @brief Default behaviour for unhandled events
             *
             */
            void react(tinyfsm::Event const&)
            {
                std::cerr << "Unhandled kafka fsm event" << std::endl;
            };

            virtual void react(EConnectToCluster const& event) {};

            // ==========================================================================
            // Entry points
            // ==========================================================================

            virtual void entry(void) = 0;
            virtual void exit(void) = 0;
        };

    } // namespace client
} // namespace celte
