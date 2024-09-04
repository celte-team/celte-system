#pragma once
#include <string>

namespace celte {
    /**
     * @brief This class is the base class of all entities managed by celte.
     * Engine integrations should include this class as either the base class
     * of their entities or wrap it as a component.
     *
     * It manages the entity's lifecycle as seen by Celte.
     */
    class CelteEntity {
    public:
        CelteEntity(double x, double y, double z);
        CelteEntity(const std::string& chunkId);
        ~CelteEntity();

    private:
        /**
         * @brief Called when the entity is spawned.
         * This is called when the entity is created and added to the scene.
         * It will generate a uuid for the entity and forward it to the chunk
         * that the entity is spawning in.
         *
         * @param x, y, z: The position of the entity in the world, necessary to
         * determine the chunk.
         */
        void __onSpawn(double x, double y, double z);

        /**
         * @brief Called when the entity is despawned.
         * This is called when the entity is removed from the scene.
         * It will forward the uuid of the entity to the chunk that the entity
         * is despawning in.
         *
         * @param chunkId: The id of the chunk that the entity is despawning in.
         */
        void __onSpawn(const std::string& chunkId);
    };
} // namespace celte