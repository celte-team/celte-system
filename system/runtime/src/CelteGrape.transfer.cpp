#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
    namespace chunks {

#ifdef CELTE_SERVER_MODE_ENABLED
        // executed on the node that drops
        void Grape::RemoteTakeEntity(const std::string& entityId)
        {
            try {

                std::cout << "[[CALL REMOTE TAKE ENTITY by grape " << _options.grapeId << "]]"
                          << std::endl;

                std::string currOwnerContainerId = ENTITIES.GetEntity(entityId).GetContainerId();

                std::string prevOwnerGrapeId = ENTITIES.GetEntity(entityId).GetOwnerChunk().GetGrapeId();

                _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId,
                    "__rp_remoteTakeEntity", entityId, prevOwnerGrapeId,
                    currOwnerContainerId);
            } catch (std::exception& e) {
                std::cerr << "Error in RemoteTakeEntity: " << e.what() << std::endl;
            }
        }

        // executed on the node that takes the entity
        bool Grape::__rp_remoteTakeEntity(const std::string& entityId,
            const std::string& prevOwnerGrapeId,
            const std::string& prevOwnerContainerId)
        {
            try {
                // TODO err handling here if entity does not exist yet!
                ENTITIES.GetEntity(entityId).ExecInEngineLoop([this, entityId,
                                                                  prevOwnerGrapeId,
                                                                  prevOwnerContainerId]() {
                    try {
                        std::cout << "[[__remote take entity]]" << std::endl;

                        // get the best container for the entity, or create it (can't refuse
                        // entity)
                        auto best = _rg.GetBestContainerForEntity(ENTITIES.GetEntity(entityId));
                        if (not best.has_value()) {
                            best = ReplicationGraph::ContainerAffinity {
                                .container = _rg.AddContainer(),
                                .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE
                            };
                        }
                        RUNTIME.IO().post([this, best, entityId, prevOwnerGrapeId,
                                              prevOwnerContainerId]() {
                            best->container->WaitNetworkInitialized();
                            ForceUpdateContainer();
                            CLOCK.ScheduleAfter(
                                _options.transferTickDelay,
                                [this, entityId, prevOwnerGrapeId, prevOwnerContainerId, best]() {
                                    ScheduleAuthorityTransfer(entityId, prevOwnerGrapeId,
                                        prevOwnerContainerId,
                                        best->container->GetId());
                                });
                        });
                    } catch (std::out_of_range& e) {
                        std::cerr << "Error in __rp_remoteTakeEntity: " << e.what() << std::endl;
                    }
                });
            } catch (std::exception& e) {
                std::cerr << "Error in __rp_remoteTakeEntity: " << e.what() << std::endl;
            }
            return true;
        }

#endif

        /* -------------------------------------------------------------------------- */
        /*                              new algo                             */
        /* -------------------------------------------------------------------------- */

        static nlohmann::json transferInfoToJson(const TransferInfo& ti)
        {
            return {
                { "entityId", ti.entityId },
                { "gFrom", ti.gFrom },
                { "cFrom", ti.cFrom },
                { "gTo", ti.gTo },
                { "cTo", ti.cTo },
            };
        }

        static TransferInfo transferInfoFromJson(const nlohmann::json& j)
        {
            return {
                .entityId = j["entityId"].get<std::string>(),
                .gFrom = j["gFrom"].get<std::string>(),
                .cFrom = j["cFrom"].get<std::string>(),
                .gTo = j["gTo"].get<std::string>(),
                .cTo = j["cTo"].get<std::string>(),
            };
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        // this is can be called either by the current owner or the new owner (entity
        // must exist locally)
        void Grape::TransferAuthority(const TransferInfo& ti)
        {
            try {
                auto& entity = ENTITIES.GetEntity(ti.entityId);
                entity.ExecInEngineLoop([this, &entity, ti]() {
                    std::string informationToLoad = entity.GetInformationToLoad();
                    std::string props = entity.GetProps();
                    std::string jsonTI = transferInfoToJson(ti).dump();

                    int tick = CLOCK.CurrentTick() + _options.transferTickDelay;
                    __containerTakes(ti.cTo, jsonTI, informationToLoad, props, tick);
                    __containerDrops(ti.cFrom, jsonTI, tick);
                });
            } catch (std::exception& e) {
                std::cerr << "Error in TransferAuthority: " << e.what() << std::endl;
            }
        }

        void Grape::__containerTakes(const std::string& topic,
            const std::string& transferInfo,
            const std::string& informationToLoad,
            const std::string& props, int tick)
        {
            _rpcs->CallVoid(tp::PERSIST_DEFAULT + topic + "." + tp::RPCs,
                "__rp_containerTakes", transferInfo, informationToLoad, props,
                tick);
        }

        void Grape::__containerDrops(const std::string& topic,
            const std::string& transferInfo, int tick)
        {
            _rpcs->CallVoid(tp::PERSIST_DEFAULT + topic + "." + tp::RPCs,
                "__rp_containerDrops", transferInfo, tick);
        }

#endif

    } // namespace chunks
} // namespace celte
