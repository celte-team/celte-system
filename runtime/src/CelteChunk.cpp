#include "CelteChunk.hpp"
#include "CelteEntityManagementSystem.hpp"
#include "CelteRPC.hpp"
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

void Chunk::__registerConsumers() {
  // A consumer to listen for Chunk scope RPCs and execute them
  KPOOL.Subscribe({
      .topic = _combinedId + "." + celte::tp::RPCs,
      .groupId = "", // no group, all consumers receive the message
      .autoCreateTopic = true,
      .autoPoll = true,
      .callback = [this](auto r) { RUNTIME.RPCTable().InvokeLocal(r); },
  });
}

bool Chunk::ContainsPosition(float x, float y, float z) const {
  return _boundingBox.ContainsPosition(x, y, z);
}

void Chunk::__registerRPCs() {
  REGISTER_RPC(__rp_scheduleEntityAuthorityTransfer,
               celte::rpc::Table::Scope::CHUNK, std::string, bool, int);
}

/* -------------------------------------------------------------------------- */
/*                                    RPCS                                    */
/* -------------------------------------------------------------------------- */

void Chunk::__rp_scheduleEntityAuthorityTransfer(std::string entityUUID,
                                                 bool take, int tick) {
  if (take) {
    CLOCK.ScheduleAt(tick, [this, entityUUID]() {
      try {
        ENTITIES.GetEntity(entityUUID).OnChunkTakeAuthority(*this);
      } catch (std::out_of_range &e) {
        std::cerr << "Entity not found: " << e.what() << std::endl;
      }
    });
  }
  // nothing to do (yet) for the chunk loosing the authority... more will come
  // in the future
}

} // namespace chunks
} // namespace celte