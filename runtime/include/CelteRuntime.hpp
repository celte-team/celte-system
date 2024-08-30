/*
** CELTE, 2024
** server-side
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** CelteRuntime.hpp
*/

#ifndef CELTE_RUNTIME_HPP
#define CELTE_RUNTIME_HPP
#ifdef CELTE_SERVER_MODE_ENABLED
#include "CelteServer.hpp"
#include "ServerStatesDeclaration.hpp"
#else
#include "CelteClient.hpp"
#include "ClientStatesDeclaration.hpp"
#endif
#include "KafkaFSM.hpp"
#include "tinyfsm.hpp"
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

namespace celte {
    namespace runtime {
        enum RuntimeMode { SERVER, CLIENT };

#ifdef CELTE_SERVER_MODE_ENABLED
        using Services
            = tinyfsm::FsmList<celte::server::AServer, celte::nl::AKafkaLink>;
#else
        using Services
            = tinyfsm::FsmList<celte::client::AClient, celte::nl::AKafkaLink>;
#endif

        /**
         * @brief This class contains all the logic necessary
         * for Celte to run in a Godot project.
         * Depending on the mode (client or server), the runtime
         * will get initialized differently to reduce checks at runtime.
         *
         */
        class CelteRuntime {
        public:
            /**
             * @brief Singleton pattern for the CelteRuntime.
             *
             */
            static CelteRuntime& GetInstance();

            CelteRuntime();
            ~CelteRuntime();

            /**
             * @brief Updates the state of the runtime synchronously.
             *
             */
            void Tick();

            /**
             * @brief Register a new callback to be executed every time the
             * Tick method is called.
             *
             * @param task
             */
            void RegisterTickCallback(std::function<void()> callback);

            /**
             * @brief Starts all Celte services
             *
             */
            void Start(RuntimeMode mode);

        private:
            // =================================================================================================
            // SERVER INIT
            // =================================================================================================
            /**
             * @brief Initialize the Celte runtime in server mode.
             *
             */
            void __initServer();

            // =================================================================================================
            // CLIENT INIT
            // =================================================================================================
            /**
             * @brief Initialize the Celte runtime in client mode.
             *
             */
            void __initClient();

            // =================================================================================================
            // PRIVATE METHODS
            // =================================================================================================

            /**
             * @brief Create the network layer, either in server or client
             * mode.
             *
             * @param mode
             */
            void __initNetworkLayer(RuntimeMode mode);

            // =================================================================================================
            // PRIVATE MEMBERS
            // =================================================================================================
            // list of tasks to be ran each frame
            std::vector<std::function<void()>> _tickCallbacks;

            RuntimeMode _mode;

            // // FSM for the client if in client mode
            // std::optional<celte::client::ServicesStates> _clientFSM
            //     = std::nullopt;

            // // FSM for the server node if in server mode
            // // TODO
        };
    } // namespace runtime
} // namespace celte

#endif // CELTE_RUNTIME_HPP