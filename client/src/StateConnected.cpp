#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include "ClientStatesDeclaration.hpp"
#include "Logger.hpp"

namespace celte {
    namespace client {
        namespace states {
            void Connected::entry() { __registerRPCs(); }

            void Connected::exit() { __unregisterRPCs(); }

            void Connected::react(EDisconnectFromServer const& event)
            {
                logs::Logger::getInstance().err() << "Disconnecting from server" << std::endl;
                transit<Disconnected>();
            }

            void Connected::__registerRPCs()
            {
                REGISTER_RPC(__rp_forceConnectToChunk, celte::rpc::Table::Scope::PEER,
                    std::string, float, float, float);
                REGISTER_RPC(__rp_spawnPlayer, celte::rpc::Table::Scope::CHUNK, std::string,
                    float, float, float);
                REGISTER_RPC(__rp_loadExistingEntities, celte::rpc::Table::Scope::PEER,
                    std::string, std::string);
            }

            void Connected::__unregisterRPCs()
            {
                UNREGISTER_RPC(__rp_forceConnectToChunk);
                UNREGISTER_RPC(__rp_spawnPlayer);
                UNREGISTER_RPC(__rp_loadExistingEntities);
            }

            void Connected::__rp_forceConnectToChunk(std::string grapeId, float x, float y,
                float z)
            {
                logs::Logger::getInstance().info()
                    << "Force connect to chunk rp has been called" << std::endl;
                // loading the map will instantiate the chunks, thus subscribing to all the
                // required topics
                HOOKS.client.grape.loadGrape(grapeId);
                // notifiying the game dev that everything is ready on our side and he may
                // request for spawn whenever
                HOOKS.client.connection.onReadyToSpawn(grapeId, x, y, z);
            }

            void Connected::__rp_spawnPlayer(std::string clientId, float x, float y,
                float z)
            {
                HOOKS.client.player.execPlayerSpawn(clientId, x, y, z);
            }

            void Connected::__rp_loadExistingEntities(std::string grapeId,
                std::string summary)
            {
                try {
                    boost::json::array summaryJSON = boost::json::parse(summary).as_array();
                    HOOKS.client.grape.onLoadExistingEntities(grapeId, summaryJSON);
                } catch (std::exception& e) {
                    logs::Logger::getInstance().err()
                        << "Error loading existing entities: " << e.what() << std::endl;
                    return;
                }
            }

        } // namespace states
    } // namespace client
} // namespace celte
