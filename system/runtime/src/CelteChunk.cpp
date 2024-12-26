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
            _createReaderStream<celte::runtime::CelteInputSystem::InputUpdate_s>({
                .thisPeerUuid = RUNTIME.GetUUID(),
                .topics = { _combinedId + "." + celte::tp::INPUT },
                .subscriptionName = RUNTIME.GetUUID() + ".input." + _combinedId,
                .exclusive = false,
                .messageHandlerSync =
                    [this](const pulsar::Consumer,
                        celte::runtime::CelteInputSystem::InputUpdate_s req) {
                        CINPUT.HandleInput(req.uuid, req.name, req.pressed, req.x, req.y);
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
                "__rp_chunkScheduleAuthorityTransfer",
                std::function([this](std::string entityUUID, std::string newOwnerChunkId,
                                  bool take, int tick) {
                    try {
                        __rp_chunkScheduleAuthorityTransfer(entityUUID, newOwnerChunkId, take,
                            tick);
                        return true;
                    } catch (std::exception& e) {
                        std::cerr << "Error in __rp_chunkScheduleAuthorityTransfer: "
                                  << e.what() << std::endl;

                        return false;
                    }
                }));
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

            // the current method is only called when the entity enters the chunk in the
            // server node, calling the RPC will trigger the behavior of transfering
            // authority over to the chunk in all the peers listening to the chunk's
            // topic.
            if (not _config.isLocallyOwned) {
                throw std::runtime_error("Cannot take entity in a non locally owned chunk");
            }
            _rpcs.CallVoid(tp::PERSIST_DEFAULT + _combinedId + "." + tp::RPCs,
                "__rp_chunkScheduleAuthorityTransfer", entityId, _combinedId,
                true, CLOCK.CurrentTick() + 30);
        }

#endif

        void Chunk::__rp_chunkScheduleAuthorityTransfer(
            const std::string& entityUUID, const std::string& newOwnerChunkId,
            bool take, int tick)
        {
#ifdef CELTE_SERVER_MODE_ENABLED
            HOOKS.server.authority.onTake(entityUUID, newOwnerChunkId);
#else
            HOOKS.client.authority.onTake(entityUUID, newOwnerChunkId);
#endif

            auto& entity = ENTITIES.GetEntity(entityUUID);
            auto& ownerContainer = entity.GetOwnerChunk(); // TODO use containers instead

#ifdef CELTE_SERVER_MODE_ENABLED
            ownerContainer.__forgetEntity(entityUUID);
#endif

            TakeEntityLocally(entityUUID);
        }

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
            std::cout << "[[CHUNK LEE]] " << _combinedId
                      << " is loading existing entities" << std::endl;
            try {
                std::string summary = _rpcs.Call<std::string>(
                    tp::PERSIST_DEFAULT + _config.grapeId,
                    "__rp_sendExistingEntitiesSummary", _combinedId);
                std::cout << "[[CHUNK LEE]] " << _combinedId
                          << "etched summary, loading entities" << std::endl;
                ENTITIES.LoadExistingEntities(summary);
            } catch (net::RPCTimeoutException& e) {
                std::cerr << "Error in LoadExistingEntities: " << e.what() << std::endl;
            }
        }

    } // namespace chunks
} // namespace celte
