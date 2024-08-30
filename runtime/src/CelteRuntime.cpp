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
** CelteRuntime.cpp
*/

#include "CelteRuntime.hpp"
#include "tinyfsm.hpp"

namespace tinyfsm {
// This registers the initial state of the FSM for
// client or server networking.
#ifdef CELTE_SERVER_MODE_ENABLED
    template <> void Fsm<celte::server::AServer>::set_initial_state()
    {
        Fsm<celte::server::AServer>::current_state_ptr
            = &_state_instance<celte::server::states::Disconnected>::value;
    }
#else
    template <> void Fsm<celte::client::AClient>::set_initial_state()
    {
        Fsm<celte::client::AClient>::current_state_ptr
            = &_state_instance<celte::client::states::Disconnected>::value;
    }
#endif
} // namespace tinyfsm

namespace celte {
    namespace runtime {
        // =================================================================================================
        // CELTE PUBLIC METHODS
        // =================================================================================================
        CelteRuntime::CelteRuntime() { }

        CelteRuntime::~CelteRuntime() { }

        void CelteRuntime::Start(RuntimeMode mode)
        {
            _mode = mode;
            __initNetworkLayer(mode);
        }

        void CelteRuntime::Tick()
        {
            for (auto& callback : _tickCallbacks) {
                callback();
            }
        }

        void CelteRuntime::RegisterTickCallback(std::function<void()> callback)
        {
            _tickCallbacks.push_back(callback);
        }

        CelteRuntime& CelteRuntime::GetInstance()
        {
            static CelteRuntime instance;
            return instance;
        }

        // =================================================================================================
        // CELTE PRIVATE METHODS
        // =================================================================================================

        void CelteRuntime::__initServer()
        {
            // TODO
        }

        void CelteRuntime::__initClient()
        {
            std::cout << "Starting client services" << std::endl;
            // celte::client::Services::start();
        }

        void CelteRuntime::__initNetworkLayer(RuntimeMode mode)
        {
            // celte::net::CelteNLOptions options {
            //     // 0 to let the OS choose the port
            //     .port = 0
            // };
            // _networkLayer.emplace(options);
        }

    } // namespace runtime
} // namespace celte