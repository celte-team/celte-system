#include "CelteInputSystem.hpp"
#include "Container.hpp"
#include "ETTRegistry.hpp"
#include "GhostSystem.hpp"
#include "Grape.hpp"
#include "PeerService.hpp"
#include "Runtime.hpp"
#include "Topics.hpp"

using namespace celte;

Entity::~Entity() {}

ETTRegistry &ETTRegistry::GetInstance() {
  static ETTRegistry instance;
  return instance;
}

void ETTRegistry::RegisterEntity(const Entity &e) {
  accessor acc;
  if (_entities.insert(acc, e.id)) {
    acc->second = e;
  } else {
    throw ETTAlreadyRegisteredException(e.id);
  }
}

void ETTRegistry::UnregisterEntity(const std::string &id) {
  accessor acc;
  if (_entities.find(acc, id)) {
    if (acc->second.isValid) {
      throw std::runtime_error(
          "Cannot unregister a valid entity as it is still in use.");
    }
    _entities.erase(acc);
    GhostSystem::TryRemoveEntity(id);
  }
}

void ETTRegistry::PushTaskToEngine(const std::string &id,
                                   std::function<void()> task) {
  accessor acc;
  if (_entities.find(acc, id)) {
    acc->second.executor.PushTaskToEngine(task);
  }
}

std::optional<std::function<void()>>
ETTRegistry::PollEngineTask(const std::string &id) {
  accessor acc;
  if (_entities.find(acc, id)) {
    return acc->second.executor.PollEngineTask();
  }
  return std::nullopt;
}

std::string ETTRegistry::GetEntityOwnerContainer(const std::string &id) {
  accessor acc;
  if (_entities.find(acc, id)) {
    return acc->second.ownerContainerId;
  }
  return "";
}

void ETTRegistry::SetEntityOwnerContainer(const std::string &id,
                                          const std::string &ownerContainer) {
  accessor acc;
  if (_entities.find(acc, id)) {
    acc->second.ownerContainerId = ownerContainer;
  }
}

bool ETTRegistry::IsEntityQuarantined(const std::string &id) {
  accessor acc;
  if (_entities.find(acc, id)) {
    return acc->second.quarantine;
  }
  return false;
}

void ETTRegistry::SetEntityQuarantined(const std::string &id, bool quarantine) {
  accessor acc;
  if (_entities.find(acc, id)) {
    acc->second.quarantine = quarantine;
  }
}

bool ETTRegistry::IsEntityLocallyOwned(const std::string &id) {
#ifdef CELTE_SERVER_MODE_ENABLED
  return ContainerRegistry::GetInstance().ContainerIsLocallyOwned(
      GetEntityOwnerContainer(id));
#else
  return false;
#endif
}

bool ETTRegistry::IsEntityValid(const std::string &id) {
  accessor acc;
  if (_entities.find(acc, id)) {
    return acc->second.isValid;
  }
  return false;
}

void ETTRegistry::SetEntityValid(const std::string &id, bool isValid) {
  accessor acc;
  if (_entities.find(acc, id)) {
    acc->second.isValid = isValid;
  }
}

void ETTRegistry::Clear() { _entities.clear(); }

void ETTRegistry::EngineCallInstantiate(const std::string &id,
                                        const std::string &payload,
                                        const std::string &ownerContainerId) {
  try {
    RegisterEntity({
        .id = id,
        .ownerContainerId = ownerContainerId,
    });
  } catch (const ETTAlreadyRegisteredException &e) {
    RunWithLock(id, [ownerContainerId](Entity &e) {
      e.ownerContainerId = ownerContainerId;
    });
    return;
  }
  RUNTIME.Hooks().onInstantiateEntity(id, payload);
  LOGGER.log(Logger::LogLevel::DEBUG, "Entity " + id + " instantiated.");
}

void ETTRegistry::LoadExistingEntities(const std::string &grapeId,
                                       const std::string &containerId) {
#ifdef DEBUG
  std::cout << "\033[032mLOAD\033[0m foreach in " << containerId.substr(0, 4)
            << std::endl;
#endif
  CallGrapeGetExistingEntities()
      .on_peer(grapeId)
      .on_fail_log_error()
      .with_timeout(std::chrono::milliseconds(10000))
      .retry(0)
      .call_async(
          std::function<void(std::map<std::string, std::string>)>(
              [this, containerId](std::map<std::string, std::string> entities) {
                for (auto &[id, data] : entities) {
                  try {
                    auto j = nlohmann::json::parse(data);
                    std::string payload = j["payload"];
                    nlohmann::json ghost = j["ghost"];
                    GHOSTSYSTEM.ApplyUpdate(id, ghost);
                    EngineCallInstantiate(id, payload, containerId);
                  } catch (const std::exception &e) {
#ifdef DEBUG
                    std::cerr << "Error while loading entity " << id << ": "
                              << e.what() << std::endl;
#endif
                    LOGERROR("Error while loading entity " + id + ": " +
                             std::string(e.what()));
                  }
                }
              }),
          containerId);
}

bool ETTRegistry::SaveEntityPayload(const std::string &eid,
                                    const std::string &payload) {
  accessor acc;
  if (_entities.find(acc, eid)) {
    acc->second.payload = payload;
    return true;
  } else {
    return false;
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
std::map<std::string, std::string>
ETTRegistry::GetExistingEntities(const std::string &containerId) {
  std::map<std::string, std::string> etts;
  for (auto &[id, e] : _entities) {
    if (e.ownerContainerId == containerId) {
      nlohmann::json j;
      j["payload"] = GetEntityPayload(id).value_or("{}");
      j["ghost"] =
          GHOSTSYSTEM.PeekProperties(id).value_or(nlohmann::json::object());
      etts.insert({id, j.dump()});
    }
  }
  return etts;
}

std::optional<std::string>
ETTRegistry::GetEntityPayload(const std::string &eid) {
  accessor acc;
  if (_entities.find(acc, eid)) {
    return acc->second.payload;
  }
  return std::nullopt;
}
#endif

void ETTRegistry::ForgetEntityNativeHandle(const std::string &id) {
#ifdef CELTE_SERVER_MODE_ENABLED
  ContainerRegistry::GetInstance().RemoveOwnedEntityFromContainer(
      GetEntityOwnerContainer(id), id);
#endif
  accessor acc;
  if (_entities.find(acc, id)) {
    acc->second.ettNativeHandle = std::nullopt;
  }
}

#ifdef CELTE_SERVER_MODE_ENABLED
void ETTRegistry::SendEntityDeleteOrder(const std::string &id) {
  RunWithLock(id, [id](Entity &e) {
    if (!e.isValid) {
      return; // already deleted
    }
    e.quarantine = true;
    e.isValid = false;

    auto payload = e.payload;
    auto ownerContainer = e.ownerContainerId;
    ContainerRegistry::GetInstance().RunWithLock(
        ownerContainer,
        [id, ownerContainer, payload](ContainerRegistry::ContainerRefCell &c) {
          if (not c.GetContainer().IsLocallyOwned()) {
            return;
          }
          RUNTIME.ScheduleAsyncIOTask([id, ownerContainer, payload]() {
            CallContainerDeleteEntity()
                .on_peer(ownerContainer)
                .on_fail_log_error()
                .fire_and_forget(id, payload);
          });
        });
  });
}
#endif

void ETTRegistry::UploadInputData(std::string uuid, std::string inputName,
                                  bool pressed, float x, float y) {
  std::string ownerChunk = GetEntityOwnerContainer(uuid);
  if (ownerChunk.empty()) {
    return; // can't send inputs if not owned by a chunk
  }
  req::InputUpdate inputUpdate;
  inputUpdate.set_name(inputName);
  inputUpdate.set_pressed(pressed);
  inputUpdate.set_uuid(uuid);
  inputUpdate.set_x(x);
  inputUpdate.set_y(y);

  CINPUT.GetWriterPool().Write<req::InputUpdate>(tp::input(ownerChunk),
                                                 inputUpdate);
}

void ETTRegistry::DeleteEntitiesInContainer(const std::string &containerId) {
  std::map<std::string, std::string> toDelete;
#ifdef DEBUG
  std::cout << "\033[031mDELETE\033[0m foreach in " << containerId.substr(0, 4)
            << std::endl;
#endif
  for (auto it = _entities.begin(); it != _entities.end(); ++it) {
    accessor acc;
    if (_entities.find(acc, it->first)) {
#ifdef DEBUG
      std::cout << "\t["
                << ((acc->second.ownerContainerId == containerId)
                        ? "\033[31mx\033[0m"
                        : "\033[032mV\033[0m")
                << "] " << it->first.substr(0, 4) << std::endl;
#endif
      if (acc->second.ownerContainerId == containerId) {
        acc->second.isValid = false;
        acc->second.quarantine = true;
        toDelete.insert({it->first, acc->second.payload});
      }
    }
    for (auto &[id, payload] : toDelete) {
      RUNTIME.TopExecutor().PushTaskToEngine([this, id, payload]() {
        RUNTIME.Hooks().onDeleteEntity(id, payload);
        ETTRegistry::UnregisterEntity(id);
      });
    }
  }
}
