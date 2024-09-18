#include "CelteChunk.hpp"
#include "CelteRuntime.hpp"
#include "glm/glm.hpp"

namespace celte {
namespace chunks {
Chunk::Chunk(const ChunkConfig &config)
    : _config(config), _combinedId(config.grapeId + "-" + config.chunkId),
      _boundingBox(config.position, config.size, config.localX, config.localY,
                   config.localZ) {
  __registerConsumers();
}

Chunk::~Chunk() {}

void Chunk::OnEntityEnter(CelteEntity &celteEntity) {}

void Chunk::OnEntityExit(CelteEntity &celteEntity) {}

void Chunk::OnEntitySpawn(CelteEntity &celteEntity) {
  std::cout << "Entity spawned in chunk " << _combinedId << std::endl;
}

void Chunk::OnEntityDespawn(CelteEntity &celteEntity) {}

void Chunk::__registerConsumers() {
  // A consumer to listen for Chunk scope RPCs and execute them
  KPOOL.Subscribe({
      .topic = _combinedId + "." + celte::tp::RPCs,
      .groupId = "", // no group, all consumers receive the message
      .autoCreateTopic = true,
      .autoPoll = true,
      .callback =
          [this](auto r) {
            std::cout << "INVOKE LOCAL IN CHUNK RPC LISTENER" << std::endl;
            RUNTIME.RPCTable().InvokeLocal(r);
          },
  });
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}

void Chunk::TakeAuthority(const std::string &entityId) {
  std::cout << "Taking authority of entity " << entityId << " in chunk "
            << _combinedId << std::endl;
}

} // namespace chunks
} // namespace celte