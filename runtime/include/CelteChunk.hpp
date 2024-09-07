#pragma once
#include <string>
#include <glm/vec3.hpp>
#include "CelteEntity.hpp"

namespace celte {
    namespace chunks {
        struct ChunkConfig {
            const std::string chunkId;
            const std::string grapeId;
            // The origin position of the chunk (ie the center of the box)
            const glm::vec3 position;
            // The size of the box in direction of the forward vector of the object holding the chunk in the engine.
            const float sizeForward;
            // The size of the box in direction of the right vector of the object holding the chunk in the engine.
            const float sizeRight;
            // The size of the box in direction of the up vector of the object holding the chunk in the engine.
            const float sizeUp;
            // The forward vector of the object holding the chunk in the engine.
            const glm::vec3 forward;
            // The up vector of the object holding the chunk in the engine.
            const glm::vec3 up;
        };

        /**
         * @brief A chunk is a region of the world which is under a unique server node's
         * control. All entities or the same chunk are replicated together: a chunk is the smallest
         * container of entities that can be moved from one server node to another.
         *
         * Chunks handle entity replication and authority over entities.
         */
        class Chunk {
        public:
            Chunk(const ChunkConfig& config);
            ~Chunk();

            /**
             * @brief Called when an entity enters the chunk.
             */
            void OnEntityEnter(CelteEntity& celteEntity);

            /**
             * @brief Called when an entity exits the chunk.
             */
            void OnEntityExit(CelteEntity& celteEntity);

            /**
             * @brief Called when an entity spawns in the chunk.
             */
            void OnEntitySpawn(CelteEntity& celteEntity);

            /**
             * @brief Called when an entity despawns in the chunk.
             */
            void OnEntityDespawn(CelteEntity& celteEntity);

            /**
             * @brief Returns true if the given position is inside the chunk.
             */
            bool ContainsPosition(float x, float y, float z) const;

        private:
            /**
             * @brief Registers all consumers for the chunk.
             * The consumers listen for events in the chunk's topic and react to them.
             */
            void __registerConsumers();

            glm::vec3 _start;
            glm::vec3 _end;
            glm::vec3 _forward;
            glm::vec3 _right;
            glm::vec3 _up;

            const ChunkConfig _config;
            const std::string _combinedId;
        };
    } // namespace chunks
} // namespace celte