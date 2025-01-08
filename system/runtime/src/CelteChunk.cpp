#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include "Requests.hpp"
#include "glm/glm.hpp"

namespace celte {
    namespace chunks {
        Chunk::Chunk(const nlohmann::json& config)
            : IEntityContainer(config["chunkId"].get<std::string>())
            , _combinedId(config["chunkId"].get<std::string>())
            , _config({
                  .chunkId = config["chunkId"],
                  .grapeId = config["grapeId"],
                  .preferredEntityCount = config["preferredEntityCount"],
                  .preferredContainerSize = config["preferredContainerSize"],
                  .isLocallyOwned = config["isLocallyOwned"],
              })
        {
            std::cout << "\n\n[[creating chunk]] " << config.dump() << "\n\n"
                      << std::endl;
        }

        Chunk::~Chunk() { }

        nlohmann::json Chunk::GetConfigJSON() const
        {
            return {
                { "chunkId", _config.chunkId },
                { "grapeId", _config.grapeId },
                { "preferredEntityCount", _config.preferredEntityCount },
                { "preferredContainerSize", _config.preferredContainerSize },
                { "isLocallyOwned", _config.isLocallyOwned },
            };
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        nlohmann::json Chunk::GetFeatures()
        {
            __refreshCentroid();
            auto& ownerGrape = GRAPES.GetGrape(_config.grapeId);
            glm::vec3 grapePosition = ownerGrape.GetPosition();
            glm::vec3 grapeSize = ownerGrape.GetSize();

            return {
                { "chunkId", _config.chunkId },
                { "grapeId", _config.grapeId },
                { "preferredEntityCount", _config.preferredEntityCount },
                { "preferredContainerSize", _config.preferredContainerSize },
                { "position", { _centroid.x, _centroid.y, _centroid.z } },
                { "isLocallyOwned", _config.isLocallyOwned },
                { "grapePosition", { grapePosition.x, grapePosition.y, grapePosition.z } },
                { "grapeSize", { grapeSize.x, grapeSize.y, grapeSize.z } },
            };
        }
#endif

        std::string Chunk::Initialize()
        {
            __registerConsumers();
            __registerRPCs();
            return _combinedId;
        }

        void Chunk::__registerConsumers()
        {
            if (not _config.isLocallyOwned) {
                _createReaderStream<req::ReplicationDataPacket>({
                    .thisPeerUuid = RUNTIME.GetUUID(),
                    .topics = { celte::tp::PERSIST_DEFAULT + _combinedId + "." + celte::tp::REPLICATION },
                    .subscriptionName = RUNTIME.GetUUID() + ".repl." + _combinedId,
                    .exclusive = false,
                    .messageHandlerSync =
                        [this](const pulsar::Consumer, req::ReplicationDataPacket req) {
                            ENTITIES.HandleReplicationData(req.data, req.active);
                        },
                });

            }

#ifdef CELTE_SERVER_MODE_ENABLED
            else { // if locally owned, we are the ones sending the data
                _replicationWS = _createWriterStream<req::ReplicationDataPacket>({
                    .topic = _combinedId + "." + celte::tp::REPLICATION,
                    .exclusive = true, // only one writer per chunk, the owner of the chunk
                });
            }
#endif
            // create a reader stream for the input system
            _createReaderStream<celte::runtime::CelteInputSystem::InputUpdateList_t>({
                .thisPeerUuid = RUNTIME.GetUUID(),
                .topics = { _combinedId + "." + celte::tp::INPUT },
                .subscriptionName = RUNTIME.GetUUID() + ".input." + _combinedId,
                .exclusive = false,
                .messageHandlerSync =
                    [this](const pulsar::Consumer,
                        celte::runtime::CelteInputSystem::InputUpdateList_t req) {
                        CINPUT.HandleInput(req);
                    },
            });
        }

        void Chunk::Remove()
        {
            std::cout << "[[REMOVING ChUNK]] " << _config.chunkId << std::endl;

            // todo : unload all entities, and then close the streams

            for (auto& rdr : _readerStreams) {
                rdr->Close();
            }
            for (auto& [_, wtr] : _writerStreams) {
                wtr->Close();
            }
            _rpcs.Close();
        }

        void Chunk::WaitNetworkInitialized()
        {
            while (not _rpcs.Ready())
                ;
            for (auto rdr : _readerStreams) {
                while (not rdr->Ready())
                    ;
            }
        }

        void Chunk::__registerRPCs()
        {
            _rpcs.Register<bool>(
                "__rp_containerTakes", std::function([this](std::string transferInfo, std::string informationToLoad, std::string props, int tick) {
                    try {
                        __rp_containerTakes(transferInfo, informationToLoad, props, tick);
                        return true;
                    } catch (std::exception& e) {
                        std::cerr << "Error in __rp_containerTakes: " << e.what()
                                  << std::endl;
                        return false;
                    }
                }));

            _rpcs.Register<bool>(
                "__rp_containerDrops",
                std::function([this](std::string transferInfo, int tick) {
                    try {
                        __rp_containerDrops(transferInfo, tick);
                        return true;
                    } catch (std::exception& e) {
                        std::cerr << "Error in __rp_containerDrops: " << e.what()
                                  << std::endl;
                        return false;
                    }
                }));

            _rpcs.Register<bool>(
                "__rp_deleteEntity",
                std::function([this](std::string entityId, std::string _) {
                    return __deleteEntity(entityId);
                }));

#ifdef CELTE_SERVER_MODE_ENABLED
            _rpcs.Register<bool>("__rp_disconnectPlayer",
                std::function([this](std::string entityId) {
                    return __rp_disconnectPlayer(entityId);
                }));
#endif
        }

        void Chunk::DisconnectPlayer(const std::string& entityId)
        {
            _rpcs.Call<bool>(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                "__rp_disconnectPlayer", entityId);
        }

        bool Chunk::__rp_disconnectPlayer(const std::string& entityId)
        {

            try {
                std::cout << "Disconnecting player id : " << entityId << std::endl;
                _rpcs.CallVoid(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                    "__rp_deleteEntity", entityId);

#ifdef CELTE_SERVER_MODE_ENABLED
                HOOKS.server.connection.onClientDisconnectedRemote(entityId);
#else
                HOOKS.client.connection.onClientDisconnectedRemote(entityId);
#endif

                return true;
            } catch (std::exception& e) {
                std::cerr << "Error in __rp_disconnectPlayer: " << e.what() << std::endl;
                return false;
            }
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        void Chunk::ScheduleReplicationDataToSend(const std::string& entityId,
            const std::string& blob,
            bool active)
        {
            if (blob.empty()) {
                return;
            }
            if (active) {
                _nextScheduledActiveReplicationData[entityId] = blob;
            } else {
                _nextScheduledReplicationData[entityId] = blob;
            }
        }

        void Chunk::SendReplicationData()
        {
            if (not _config.isLocallyOwned)
                return;

            // note: std move clears the map in place so we don't need to clear it
            // manually :)
            if (not _nextScheduledReplicationData.empty()) {
                _replicationWS->Write<req::ReplicationDataPacket>(
                    req::ReplicationDataPacket {
                        .data = std::move(_nextScheduledReplicationData),
                        .active = false,
                    });
            }

            if (not _nextScheduledActiveReplicationData.empty()) {
                _replicationWS->Write<req::ReplicationDataPacket>(
                    req::ReplicationDataPacket {
                        .data = std::move(_nextScheduledActiveReplicationData),
                        .active = true,
                    });
            }
        }

        void Chunk::TakeEntity(const std::string& entityId)
        {
            if (!_config.isLocallyOwned) {
                throw std::runtime_error(
                    "Cannot take entity globally in a non locally owned chunk");
            }

            std::string prevOwnerContainerId = ENTITIES.GetEntity(entityId).GetContainerId();
            auto& ownerGrape = GRAPES.GetGrape(_config.grapeId);
            ownerGrape.ScheduleAuthorityTransfer(entityId, _config.grapeId,
                prevOwnerContainerId, _combinedId);
        }

#endif

        /* --------------------------------------------------------------------------
         */
        /*                                    RPCS */
        /* --------------------------------------------------------------------------
         */

        void Chunk::ScheduleEntityAuthorityTransfer(std::string entityUUID,
            std::string newOwnerChunkId,
            int tick)
        {

#ifdef CELTE_SERVER_MODE_ENABLED
            HOOKS.server.authority.onTake(entityUUID, newOwnerChunkId);
#else
            HOOKS.client.authority.onTake(entityUUID, newOwnerChunkId);
#endif

            auto& entity = ENTITIES.GetEntity(entityUUID);
            auto& ownerContainer = entity.GetOwnerChunk(); // TODO use containers instead

#ifdef CELTE_SERVER_MODE_ENABLED
            // TODO : @ewen, call on rpc on this chunk's rpc channel to forget the entity
            // also, forgetting the entity should delete it in peers that are not taking
            // the entity in another container (the new owner is not replicated on the
            // peer)

            ownerContainer.__forgetEntity(entityUUID);
#endif

            TakeEntityLocally(entityUUID);
        }

        void Chunk::TakeEntityLocally(const std::string& entityId)
        {
            try {
                ENTITIES.GetEntity(entityId).OnChunkTakeAuthority(*this);
                std::cout << "[[TAKE ENTITY LOCALLY]] container " << _combinedId << " took "
                          << entityId << std::endl;
#ifdef CELTE_SERVER_MODE_ENABLED
                if (_config.isLocallyOwned) {
                    __rememberEntity(entityId);
                }
#endif
            } catch (std::out_of_range& e) {
                std::cerr << "Error in TakeEntityLocally: " << e.what() << std::endl;
            }
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        void Chunk::__forgetEntity(const std::string& entityId)
        {
            _ownedEntities.erase(entityId);
        }

        void Chunk::__rememberEntity(const std::string& entityId)
        {
            _ownedEntities.insert(entityId);
        }
#endif

        void Chunk::__attachEntityAsync(const std::string& entityId, int retries)
        {
            if (retries <= 0) {
                std::cerr << "Entity spawn timed out, won't spawn on the network"
                          << std::endl;
                return;
            }
            if (not ENTITIES.IsEntityRegistered(entityId)) {
                CLOCK.ScheduleAfter(10, [this, entityId, retries]() {
                    __attachEntityAsync(entityId, retries - 1);
                });
            }
            TakeEntityLocally(entityId);
        }

        void Chunk::SetEntityPositionGetter(
            std::function<glm::vec3(const std::string&)> getter)
        {
            _entityPositionGetter = getter;
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        void Chunk::__refreshCentroid()
        {
            if (_entityPositionGetter == nullptr) {
                std::cerr << "Entity position getter not set, cannot refresh centroid"
                          << std::endl;
                return;
            }
            glm::vec3 sum = glm::vec3(0);
            unsigned int n = 0;
            for (const auto& entityId : _ownedEntities) {
                sum += _entityPositionGetter(entityId);
                n++;
            }
            if (n == 0) {
                return;
            }
            _centroid = sum / static_cast<float>(n);
        }
#endif

        void Chunk::Load(const nlohmann::json& features)
        {
            _config.preferredContainerSize = features["preferredContainerSize"];
            _config.preferredEntityCount = features["preferredEntityCount"];
            _config.isLocallyOwned = false; // if you have to load feats, you are not
                                            // the owner of the chunk
        }

        void Chunk::LoadExistingEntities()
        {
            try {
                std::string summary = _rpcs.Call<std::string>(
                    tp::PERSIST_DEFAULT + _config.grapeId,
                    "__rp_sendExistingEntitiesSummary", _combinedId);
                ENTITIES.LoadExistingEntities(summary);
            } catch (net::RPCTimeoutException& e) {
                // retry the operation
                RUNTIME.IO().post([this]() { LoadExistingEntities(); });
            }
        }

        // runs in the container that is taking the entity
        void Chunk::__rp_containerTakes(const std::string& transferInfo,
            const std::string& informationToLoad,
            const std::string& props, int tick)
        {
            nlohmann::json j = nlohmann::json::parse(transferInfo);

            std::cout << "container " << j["cTo"] << " [[TAKES]] " << j["entityId"]
                      << std::endl;

            if (not ENTITIES.IsEntityRegistered(j["entityId"].get<std::string>())) {
                GRAPES.GetGrape(j["gTo"]).InstantiateEntityLocally(
                    j["entityId"].get<std::string>(), informationToLoad, props);
            }

            CLOCK.ScheduleAt(tick, [this, j]() {
                __attachEntityAsync(j["entityId"].get<std::string>(), 10);
            });
        }

        void Chunk::__rp_containerDrops(const std::string& transferInfo, int tick)
        {
            CLOCK.ScheduleAt(tick, [this, transferInfo]() {
                // if cTo does not exist here, we must remove the entity
                nlohmann::json j = nlohmann::json::parse(transferInfo);

                std::cout << "container " << j["cFrom"] << " [[DROPS]] " << j["entityId"]
                          << std::endl;
                std::cout << "\t(context: " << _combinedId << ")" << std::endl;

#ifdef CELTE_SERVER_MODE_ENABLED
                __forgetEntity(j["entityId"]);
#endif
                // if the new owner has not taken the entity yet, we must remove it
                // from this chunk to avoid double assignment over different peers
                try {
                    auto& entity = ENTITIES.GetEntity(j["entityId"]);
                    CelteEntity::OwnerContainerLock lock(entity);
                    auto ownerId = entity.GetContainerIdUnsafe();
                    if (ownerId == j["cFrom"]) {
                        entity.ResetOwnerContainerUnsafe();
                    }
                } catch (std::out_of_range& e) {
                    // container not set
                }
                if (not GRAPES.GetGrape(j["gTo"])
                            .GetReplicationGraph()
                            .GetContainerOpt(j["cTo"])
                            .has_value()) {
                    // TODO @ewen remove entity
                }
            });
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        void Chunk::DeleteEntity(const std::string& entityId)
        {
            _rpcs.Call<bool>(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                "__rp_deleteEntity", entityId);
        }
#endif

        bool Chunk::__deleteEntity(const std::string& entityId)
        {
            try {
                std::shared_ptr<CelteEntity> entity = ENTITIES.GetEntityPtr(entityId);
                if (entity == nullptr) {
                    std::cerr << "Erase: No entity in the list with this id : " << entityId
                              << std::endl;
                    return false;
                }
                // remove from network
                ENTITIES.QuaranteenEntity(entityId);
                ENTITIES.UnregisterEntity(entity);
                CINPUT.GetListInput()->erase(entityId);

                entity->ExecInEngineLoop([this, entity, entityId]() {
                    void* apiWrapper = entity->GetWrapper();
#ifdef CELTE_SERVER_MODE_ENABLED
                    __forgetEntity(entityId);
                    HOOKS.server.entity.onDelete(apiWrapper);
#else
                    HOOKS.client.entity.onDelete(apiWrapper);
#endif
                    std::cout << "Succesfully delete entity id : " << entity->GetUUID()
                              << std::endl;
                    ENTITIES.UnquaranteenEntity(entity->GetUUID());
                });

                return true;
            } catch (std::exception& e) {
                std::cerr << "Error in __deleteEntity: " << e.what() << std::endl;
                return false;
            }
        }

    } // namespace chunks
} // namespace celte
