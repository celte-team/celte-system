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
  // runtime::CelteRuntime::GetInstance().KPool().Subscribe(
  //     _combinedId + ".rpc",
  //     [this](kafka::clients::consumer::ConsumerRecord record) {
  //       runtime::CelteRuntime::GetInstance().RPCTable().InvokeLocal(record);
  //     });
  RUNTIME.KPool().Subscribe({
      .topic = _combinedId + ".rpc",
      .groupId = _combinedId + ".rpc",
      .autoCreateTopic = true,
      .autoPoll = true,
      .callback = [this](auto r) { RUNTIME.RPCTable().InvokeLocal(r); },
  });
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}
} // namespace chunks
} // namespace celte