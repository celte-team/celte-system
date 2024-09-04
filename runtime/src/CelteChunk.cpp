#include "CelteChunk.hpp"

namespace celte {
    namespace chunks {
        Chunk::Chunk(const ChunkConfig &config)
            : _config(config),
            _combinedId(config.grapeId + "-" + config.chunkId)
        {
        }

        Chunk::~Chunk() { }

        void Chunk::OnEntityEnter(CelteEntity &celteEntity) { }

        void Chunk::OnEntityExit(CelteEntity &celteEntity) { }

        void Chunk::OnEntitySpawn(CelteEntity &celteEntity) { }

        void Chunk::OnEntityDespawn(CelteEntity &celteEntity) { }
    } // namespace chunks
} // namespace celte