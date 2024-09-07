#pragma once
#include "CelteChunk.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <memory>
#include <glm/vec3.hpp>
#include <vector>

namespace celte {
    namespace chunks {
        /**
         * Use these options to configure a grape.
         * These options should be available in the engine's editor,
         * wrapped by a class in the engine's celte api.
         */
        struct GrapeOptions {
            const std::string grapeId;
            const int subdivision;
            // The origin position of the grape (ie the center of the box)
            const glm::vec3 position;
            // The size of the box in direction of the forward vector of the object holding the grape in the engine.
            const float sizeForward;
            // The size of the box in direction of the right vector of the object holding the grape in the engine.
            const float sizeRight;
            // The size of the box in direction of the up vector of the object holding the grape in the engine.
            const float sizeUp;
            // The forward vector of the object holding the grape in the engine.
            const glm::vec3 forward;
            // The up vector of the object holding the grape in the engine.
            const glm::vec3 up;
        };

        struct GrapeStatistics {
            const std::string grapeId;
            const size_t numberOfChunks;
            std::vector<std::string> chunksIds;
        };

        /**
         * @brief A grape is a region of the world that is managed by a single
         * server node. Grapes are subdivided into
         * chunks to allow reassigning overloaded areas to new server nodes.
         * This is doable by using the grape constructor that takes a grape as
         * an argument.
         */
        class Grape {
        public:
            Grape(const GrapeOptions& options);

            /**
             * @brief Construct a grape from another grape.
             * This constructor is used to create a new grape that is a
             * subdivision of an existing grape. The chunks of the new grape
             * will be removed from the previous grape.
             *
             * @param grape: The grape to subdivide
             * @param chunks: The chunks to remove from the previous grape and
             * add to the new one.
             */
            Grape(Grape& grape, std::vector<std::string> chunksIds);
            ~Grape();

            /**
             * @brief Returns statistics and informations about the Grape.
             * This method is used to display information about the grape in the
             * engine's editor or for debugging purposes.
             */
            GrapeStatistics GetStatistics() const;

            /**
             * @brief Returns true if the given position is inside the grape.
             */
            bool ContainsPosition(float x, float y, float z) const;

            /**
             * @brief Returns the id of the grape.
             */
            inline const std::string& GetGrapeId() const { return _options.grapeId; }

            /**
             * @brief Returns the chunk at the given position.
             * This method is used to forward entities to the correct chunk
             * when they are spawned.
             */
            Chunk& GetChunkByPosition(float x, float y, float z);

        private:
            /**
             * Subdivide the grape bounding box into options.subdivision chunks.
             */
            void __subdivide();

            const GrapeOptions _options;
            std::unordered_map<std::string, std::shared_ptr<Chunk>> _chunks;

            glm::vec3 _start;
            glm::vec3 _end;
            glm::vec3 _forward;
            glm::vec3 _right;
            glm::vec3 _up;
        };
    } // namespace chunks
} // namespace celte