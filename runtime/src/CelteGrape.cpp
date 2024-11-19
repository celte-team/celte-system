#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace celte {
    namespace chunks {

        Grape::Grape(const GrapeOptions& options)
            : _options(options)
        {
            if (_options.subdivision <= 0) {
                throw std::invalid_argument("Subdivision must be a positive integer.");
            }
            logs::Logger::getInstance().info() << "Subdividing grape...";
            try {
                __subdivide();
                logs::Logger::getInstance().info()
                    << "Grape " << _options.grapeId << " created.";
            } catch (std::exception& e) {
                logs::Logger::getInstance().err()
                    << "Error, could not subdivide grape: " << e.what();
            }
            logs::Logger::getInstance().info()
                << "Grape " << _options.grapeId << " created.";
        }

        Grape::Grape(Grape& grape, std::vector<std::string> chunksIds)
            : _options(grape._options)
        {
            for (auto chunkId : chunksIds) {
                _chunks[chunkId] = grape._chunks[chunkId];
                grape._chunks.erase(chunkId);
            }
        }

        Grape::~Grape() { }

        void Grape::__subdivide()
        {
            // subdivide each axis into _options.subdivision parts to create a list of
            // points equally spaced along each axis, to map the space in the grape
            RotatedBoundingBox boundingBox(_options.position, _options.size,
                _options.localX, _options.localY,
                _options.localZ);
            auto points = boundingBox.GetMeshedPoints(_options.subdivision);

            std::vector<std::string> rpcTopics;
            std::vector<std::string> inputTopics;
            std::vector<std::string> replTopics;

            // create a chunk for each point
            for (auto point : points) {
                std::stringstream chunkId;
                glm::ivec3 pointInt = glm::ivec3(point);
                chunkId << "." << pointInt.x << "." << pointInt.y << "." << pointInt.z;
                ChunkConfig config = { .chunkId = chunkId.str(),
                    .grapeId = _options.grapeId,
                    .position = pointInt,
                    .localX = _options.localX,
                    .localY = _options.localY,
                    .localZ = _options.localZ,
                    .size = _options.size / (float)_options.subdivision,
                    .isLocallyOwned = _options.isLocallyOwned };
                _chunks[chunkId.str()] = std::make_shared<Chunk>(config);
                std::string combinedId = _chunks[chunkId.str()]->Initialize();

                rpcTopics.push_back(combinedId + "." + tp::RPCs);
                inputTopics.push_back(combinedId + "." + tp::INPUT);
                replTopics.push_back(combinedId + "." + tp::REPLICATION);
            }

// Server creates replication topics
#ifdef CELTE_SERVER_MODE_ENABLED
            KPOOL.CreateTopicsIfNotExist(replTopics, 1, 1);
            KPOOL.CreateTopicsIfNotExist(inputTopics, 1, 1);
            KPOOL.CreateTopicsIfNotExist(rpcTopics, 1, 1);
#endif

            // Subscribe to chunk rpc topics. This is common between server and client.
            KPOOL.Subscribe({
                .topics = rpcTopics, .groupId = "", .autoCreateTopic = false,
                // callbacks are already set in the chunk Initialize method
            });
            KPOOL.CommitSubscriptions();

            // Client consumer from replication topic (or server if not locally owned)
            if (not _options.isLocallyOwned) {
                // Replication needs to be reactive so we'll use a dedicated thread
                KPOOL.Subscribe({ .topics = replTopics,
                    .groupId = "",
                    .autoCreateTopic = false,
                    .useDedicatedThread = true });
                ENTITIES.RegisterReplConsumer(replTopics);
                KPOOL.CommitSubscriptions();
            }

            KPOOL.Subscribe({ .topics = inputTopics,
                .groupId = "",
                .autoCreateTopic = false,
                .useDedicatedThread = true });
            CINPUT.RegisterInputCallback(inputTopics);
            KPOOL.CommitSubscriptions();
        }

        GrapeStatistics Grape::GetStatistics() const
        {
            GrapeStatistics stats = { .grapeId = _options.grapeId,
                .numberOfChunks = _chunks.size() };
            for (auto& [chunkId, chunk] : _chunks) {
                stats.chunksIds.push_back(chunkId);
            }
            return stats;
        }

        bool Grape::ContainsPosition(float x, float y, float z) const
        {
            for (auto& [chunkId, chunk] : _chunks) {
                if (chunk->ContainsPosition(x, y, z)) {
                    return true;
                }
            }
            return false;
        }

        Chunk& Grape::GetChunkByPosition(float x, float y, float z)
        {
            for (auto& [chunkId, chunk] : _chunks) {
                if (chunk->ContainsPosition(x, y, z)) {
                    return *chunk;
                }
            }
            throw std::out_of_range("Position (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ") is not in grape " + _options.grapeId);
        }

#ifdef CELTE_SERVER_MODE_ENABLED
        void Grape::ReplicateAllEntities()
        {
            if (not _options.isLocallyOwned) {
                return;
            }
            for (auto& [chunkId, chunk] : _chunks) {
                chunk->SendReplicationData();
            }
        }
#endif

    } // namespace chunks
} // namespace celte
