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
#include "CelteEntityManagementSystem.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "tinyfsm.hpp"
#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace tinyfsm {
// This registers the initial state of the FSM for
// client or server networking.
#ifdef CELTE_SERVER_MODE_ENABLED
    template <>
    void Fsm<celte::server::AServer>::set_initial_state()
    {
        Fsm<celte::server::AServer>::current_state_ptr = &_state_instance<celte::server::states::Disconnected>::value;
    }
#else
    template <>
    void Fsm<celte::client::AClient>::set_initial_state()
    {
        Fsm<celte::client::AClient>::current_state_ptr = &_state_instance<celte::client::states::Disconnected>::value;
    }
#endif
} // namespace tinyfsm

namespace celte {
    namespace runtime {

        // =================================================================================================
        // CELTE PUBLIC METHODS
        // =================================================================================================
        CelteRuntime::CelteRuntime()
            : _pool(nullptr)
        {
            _inputs = std::make_shared<celte::runtime::CelteInputSystem>();
        }

        CelteRuntime::~CelteRuntime() { }

        rpc::Table& CelteRuntime::RPCTable() { return _rpcTable; }

        void CelteRuntime::Start(RuntimeMode mode)
        {
            _mode = mode;
            __initNetworkLayer(mode);
        }

        void CelteRuntime::Tick()
        {
            // Executing tasks received from the network
            if (_pool) [[likely]] {
                // _pool->CommitSubscriptions();
                _pool->CatchUp();
            }
            // Executing tasks that have been scheduled for delayed execution
            _clock.CatchUp();
            for (auto& callback : _tickCallbacks) {
                // callback.first is the uuid of the callback
                callback.second();
            }
            ENTITIES.Tick();
        }

        int CelteRuntime::RegisterTickCallback(std::function<void()> callback)
        {
            _tickCallbacks.insert(std::make_pair(++_tickCallbackId, callback));
            return _tickCallbackId;
        }

        void CelteRuntime::UnregisterTickCallback(int id)
        {
            // documentation says that no exception is thrown if the key is not found
            _tickCallbacks.erase(id);
        }

        CelteRuntime& CelteRuntime::GetInstance()
        {
            static CelteRuntime instance;
            return instance;
        }

        const std::string& CelteRuntime::GetUUID() const
        {
#ifdef CELTE_SERVER_MODE_ENABLED
            static const std::string PEER_UUID = "sn." + boost::uuids::to_string(boost::uuids::random_generator()()); // random uuid for the
                                                                                                                      // peer to identify
                                                                                                                      // itself to the master
#else
            static const std::string PEER_UUID = "client." + boost::uuids::to_string(boost::uuids::random_generator()()); // random uuid for the peer to
                                                                                                                          // identify itself to the
                                                                                                                          // master
#endif
            return PEER_UUID;
        }

// =================================================================================================
// CELTE PRIVATE METHODS
// =================================================================================================
#ifdef CELTE_SERVER_MODE_ENABLED
        void CelteRuntime::__initServerRPC() { }

        void CelteRuntime::__initServer() { __initServerRPC(); }

#endif

#ifndef CELTE_SERVER_MODE_ENABLED

        void CelteRuntime::__initClientRPC() { }

        void CelteRuntime::__initClient() { __initClientRPC(); }

        void CelteRuntime::RequestSpawn(const std::string& clientId,
            const std::string& grapeId, float x, float y,
            float z)
        {
            // TODO: check if spawn is authorized
            RPC.InvokeGrape(grapeId, "__rp_onSpawnRequested", clientId, x, y, z);
        }

#endif

        void CelteRuntime::__initNetworkLayer(RuntimeMode mode)
        {
            Services::start();

            switch (mode) {
            case SERVER:
#ifdef CELTE_SERVER_MODE_ENABLED
                Info() << "CelteRuntime is starting in server mode" << std::endl;
                __initServer();
#endif
                break;
            case CLIENT:
                Info() << "CelteRuntime is starting in client mode" << std::endl;
#ifndef CELTE_SERVER_MODE_ENABLED
                __initClient();
#endif
                break;
            default:
                break;
            }
        }

        void CelteRuntime::ConnectToCluster()
        {
            std::string ip = std::getenv("CELTE_CLUSTER_HOST");
            if (ip.empty()) {
                throw std::runtime_error("CELTE_CLUSTER_HOST not set");
            }
            int port = 80;
            ConnectToCluster(ip, port);
        }

        void CelteRuntime::ConnectToCluster(const std::string& ip, int port)
        {
            _pool = std::make_shared<celte::nl::KPool>(celte::nl::KPool::Options {
                .bootstrapServers = ip + std::string(":") + std::to_string(port) });

            // Launch the connection asynchronously
            Services::dispatch(celte::EConnectToCluster {
                .ip = ip,
                .port = port,
                .message = std::make_shared<std::string>("hello") });
        }

        bool CelteRuntime::IsConnectedToCluster()
        {
#ifdef CELTE_SERVER_MODE_ENABLED
            return tinyfsm::Fsm<celte::server::AServer>::is_in_state<
                celte::server::states::Connected>();
#else
            return tinyfsm::Fsm<celte::client::AClient>::is_in_state<
                celte::client::states::Connected>();
#endif
        }

        bool CelteRuntime::IsConnectingToCluster()
        {
#ifdef CELTE_SERVER_MODE_ENABLED
            return tinyfsm::Fsm<celte::server::AServer>::is_in_state<
                celte::server::states::Connecting>();
#else
            return tinyfsm::Fsm<celte::client::AClient>::is_in_state<
                celte::client::states::Connecting>();
#endif
        }

        bool CelteRuntime::WaitForClusterConnection(int timeoutMs)
        {
            auto start = std::chrono::high_resolution_clock::now();
            while (true) {
                if (IsConnectedToCluster()) {
                    return true;
                }
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                if (duration.count() > timeoutMs) {
                    return false;
                }
            }
        }

        // nl::KafkaPool &CelteRuntime::KPool() {
        nl::KPool& CelteRuntime::KPool()
        {
            if (!_pool) {
                throw std::logic_error("Kafka pool not initialized");
            }
            return *_pool;
        }

        CelteInputSystem& CelteRuntime::CelteInput()
        {
            if (!_inputs) {
                throw std::logic_error("Kafka pool not initialized");
            }
            return *_inputs;
        }

        api::HooksTable& CelteRuntime::Hooks() { return _hooks; }

        std::ostream& CelteRuntime::Info()
        {
            return logs::Logger::getInstance().info();
        }

        std::ostream& CelteRuntime::Err() { return logs::Logger::getInstance().err(); }

        logs::Logger& CelteRuntime::Logger() { return logs::Logger::getInstance(); }

        celte::chunks::CelteGrapeManagementSystem& CelteRuntime::GetGrapeManager()
        {
            return celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER();
        }

    } // namespace runtime
} // namespace celte
