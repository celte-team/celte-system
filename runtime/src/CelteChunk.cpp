#include "CelteChunk.hpp"
#include "KafkaFSM.hpp"
#include "CelteRuntime.hpp"
#include "glm/glm.hpp"

namespace celte {
    namespace chunks {
        Chunk::Chunk(const ChunkConfig &config)
            : _config(config),
            _combinedId(config.grapeId + "-" + config.chunkId)
        {
            _forward = glm::normalize(config.forward);
            _right = glm::normalize(glm::cross(_forward, config.up));
            _up = glm::normalize(config.up);
            _start = config.position - config.forward * config.sizeForward / 2.0f - _right * config.sizeRight / 2.0f - _up * config.sizeUp / 2.0f;
            _end = config.position + config.forward * config.sizeForward / 2.0f + _right * config.sizeRight / 2.0f + _up * config.sizeUp / 2.0f;

            __registerConsumers();
        }

        Chunk::~Chunk() { }

        void Chunk::OnEntityEnter(CelteEntity &celteEntity) { }

        void Chunk::OnEntityExit(CelteEntity &celteEntity) { }

        void Chunk::OnEntitySpawn(CelteEntity &celteEntity) {
            std::cout << "Entity spawned in chunk " << _combinedId << std::endl;
        }

        void Chunk::OnEntityDespawn(CelteEntity &celteEntity) { }

        void Chunk::__registerConsumers() {
            // A consumer to listen for Chunk scope RPCs and execute them
            nl::AKafkaLink::CreateTopicIfNotExists(_combinedId + ".rpc");
            nl::AKafkaLink::RegisterConsumer(_combinedId + ".rpc",
                [this](kafka::clients::consumer::ConsumerRecord record) {
                    rpc::TABLE().InvokeLocal(record);
                });
        }

        bool Chunk::ContainsPosition(float x, float y, float z) const {
            return x >= _start.x && x <= _end.x
                && y >= _start.y && y <= _end.y
                && z >= _start.z && z <= _end.z;
        }
    } // namespace chunks
} // namespace celte