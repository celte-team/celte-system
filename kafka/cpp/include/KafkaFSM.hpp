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
#include <chrono>
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
                std::shared_ptr<kafka::clients::consumer::KafkaConsumer>
                    consumer;
                std::queue<
                    std::vector<kafka::clients::consumer::ConsumerRecord>>
                    recvdData;
                ScheduledKCTask dataHandler;
                std::shared_ptr<std::mutex> mutex
                    = std::make_shared<std::mutex>();
            };

        public:
            struct KCelteConfig {
                // Timeout for consumer's poll method
                std::chrono::milliseconds pollTimeout;
                // Interval between two polls. Real time between two polls may
                // be greater than this, but not less.
                std::chrono::milliseconds pollingIntervalMs;
            };

            /**
             * @brief This map stores all consumers registered by the client or
             * the server.
             */
            static std::unordered_map<std::string, Consumer> _consumers;

            /**
             * @brief Default properties for the kafka consumers.
             * All properties defined using this object will be applied to all
             * consumers created by the client or the server.
             */
            static kafka::Properties kDefaultProps;

            /**
             * @brief Default configuration for the kafka link.
             * This does not affect the kafka consumers or producers directly,
             * but might affect the way they are created or used by KafkaLink.
             */
            static KCelteConfig kCelteConfig;

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
             * @brief Removes a consumer from the list of consumers using its
             * topic as the key.
             */
            static void UnregisterConsumer(const std::string& topic);

            /**
             * Executes all tasks scheduled from the received messages.
             * KafkaLink is not the one responsible for calling this method!
             * It should be called by the runtime, synchronously.
             */
            static void Catchback();

            /**
             * @brief Clears all consumers and their data, without processing
             * it.
             */
            static void ClearAllConsumers();

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

        protected:
            /**
             * @brief Polls all consumers for new messages and pushes
             * any received message to the consumer's queue.
             */
            static void __pollAllConsumers();
        };

    } // namespace client
} // namespace celte
