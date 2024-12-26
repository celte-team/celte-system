#include "CelteGrape.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace celte {
namespace chunks {

#ifdef CELTE_SERVER_MODE_ENABLED
bool Grape::__rp_onSpawnRequested(std::string &clientId, std::string &payload) {
  std::cout << "[[on spawn requested]]" << std::endl;
  try {
    auto [_, x, y, z] = ENTITIES.GetPendingSpawn(clientId);
    ENTITIES.RemovePendingSpawn(clientId);
    RUNTIME.IO().post([this, clientId, payload, x, y, z]() {
      __ownerExecEntitySpawnProcess(clientId, payload, x, y, z);
    });
  } catch (std::out_of_range &e) {
    std::cerr << "Error in "
                 "__rp_onSpawnRequested: "
              << e.what() << std::endl;
    return false;
  }
  return true;
}

void Grape::__ownerExecEntitySpawnProcess(const std::string &entityId,
                                          const std::string &payload, float x,
                                          float y, float z) {
  __spawnEntityLocally(
      entityId, payload, glm::vec3(x, y, z),
      [this, entityId, payload, x, y,
       z]() { // then, when godot is ready
        auto &entity = ENTITIES.GetEntity(entityId);
        entity.ExecInEngineLoop([this, &entity, entityId, payload, x, y, z]() {
          auto containerOpt = _rg.GetBestContainerForEntity(entity);
          std::shared_ptr<IEntityContainer> container;
          if (containerOpt.has_value()) {
            container = containerOpt.value().container;
          } else {
            auto newContainerAffinity = ReplicationGraph::ContainerAffinity{
                .container = _rg.AddContainer(),
                .affinity = ReplicationGraph::DEFAULT_AFFINITY_SCORE};
            container = newContainerAffinity.container;
          }
          container->WaitNetworkInitialized();
          container->TakeEntityLocally(entityId);
          __spawnEntityOnNetwork(entityId, container->GetId(), payload, x, y,
                                 z);
        });
      });
}

void Grape::__spawnEntityOnNetwork(const std::string &entityId,
                                   const std::string &containerId,
                                   const std::string &payload, float x, float y,
                                   float z) {
  std::cout << "[[spawn entity on network]]" << std::endl;
  _rpcs->CallVoid(tp::PERSIST_DEFAULT + _options.grapeId + "." + tp::RPCs,
                  "__rp_spawnEntity", entityId, containerId, payload, x, y, z);
}

#endif

void Grape::__execEntitySpawnProcess(const std::string &entityId,
                                     const std::string &containerId,
                                     const std::string &payload, float x,
                                     float y, float z) {
  std::cout << "[[exec entity spawn process]]" << std::endl;
  if (_options.isLocallyOwned) {
    return; // done already in
            // ownerExecEntitySpawnProcess
  } else {
    __spawnEntityLocally(entityId, payload, glm::vec3(x, y, z),
                         [this, entityId, containerId, x, y, z]() {
                           auto containerOpt = _rg.GetContainerOpt(containerId);
                           std::shared_ptr<IEntityContainer> container;
                           if (containerOpt.has_value()) {
                             container = containerOpt.value();
                           } else {
                             container = _rg.AddContainer(containerId);
                           }
                           container->WaitNetworkInitialized();
                           container->TakeEntityLocally(entityId);
                         });
  }
}

void Grape::__spawnEntityLocally(const std::string &entityId,
                                 const std::string &payload, glm::vec3 position,
                                 std::function<void()> then) {
  std::cout << "[[spawn entity locally]]" << std::endl;
  __callSpawnHook(entityId, payload, position);
  __waitEntityReady(entityId, then);
}

void Grape::__waitEntityReady(const std::string &entityId,
                              std::function<void()> then) {
  if (not ENTITIES.IsEntityRegistered(entityId)) {
    RUNTIME.IO().post(
        [this, entityId, then]() { __waitEntityReady(entityId, then); });
    return;
  }
  try {
    then();
  } catch (std::exception &e) {
    std::cerr << "Error in __waitEntityReady: " << e.what() << std::endl;
  }
}

void Grape::__callSpawnHook(const std::string &entityId,
                            const std::string &payload, glm::vec3 position) {
  std::cout << "[[call spawn hook]]" << std::endl;
  RUNTIME.ExecInEngineLoop([this, entityId, payload, position]() {
    try {
#ifdef CELTE_SERVER_MODE_ENABLED
      HOOKS.server.newPlayerConnected.execPlayerSpawn(
          entityId, payload, position.x, position.y, position.z);
#else
      HOOKS.client.player.execPlayerSpawn(entityId, payload, position.x,
                                          position.y, position.z);
#endif
    } catch (std::exception &e) {
      std::cerr << "Error in __callSpawnHook: " << e.what() << std::endl;
    }
  });
}

} // namespace chunks
} // namespace celte