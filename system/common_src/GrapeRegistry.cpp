#include "GrapeRegistry.hpp"

using namespace celte;

GrapeRegistry &GrapeRegistry::GetInstance() {
  static GrapeRegistry instance;
  return instance;
}

void GrapeRegistry::RegisterGrape(const std::string &grapeId,
                                  bool isLocallyOwned,
                                  std::function<void()> onReady) {
  accessor acc;
  if (not _grapes.insert(acc, grapeId))
    throw std::runtime_error("Grape with id " + grapeId + " already exists.");

  acc->second.id = grapeId;
  acc->second.isLocallyOwned = isLocallyOwned;
#ifdef CELTE_SERVER_MODE_ENABLED
  if (isLocallyOwned) {
    acc->second.clientRegistry.emplace(); // create the client registry
    acc->second.clientRegistry->StartKeepAliveThread();
  }
#endif
  acc.release();

  if (onReady) {
    RUNTIME.ScheduleAsyncTask([onReady, grapeId]() {
      // wait for the rpc service to be ready
      accessor acc2;
      if (GRAPES.GetGrapes().find(acc2, grapeId)) {
        while (!acc2->second.rpcService.has_value() and
               not acc2->second.rpcService->Ready()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        acc2.release();
        GRAPES.PushTaskToEngine(grapeId, onReady);
      }
    });
  }
}

void GrapeRegistry::UnregisterGrape(const std::string &grapeId) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    _grapes.erase(acc);
  }
}

std::string
GrapeRegistry::ContainerCreateAndAttach(std::string grapeId,
                                        std::function<void()> onReady) {
  Container *container = new Container();
  if (not container->AttachToGrape(grapeId))
    return "error-bad-grape";
  container->WaitForNetworkReady([container, onReady](bool ready) {
    if (ready) {
      RUNTIME.TopExecutor().PushTaskToEngine([onReady]() { onReady(); });
    }
  });
  return container->GetId();
}

bool GrapeRegistry::ContainerExists(const std::string &containerId) {
  for (auto &[_, grape] : _grapes) {
    decltype(grape.containers)::accessor acc;
    if (grape.containers.find(acc, containerId)) {
      return true;
    }
  }
  return false;
}

std::optional<std::string>
GrapeRegistry::GetOwnerOfContainer(const std::string &containerId) {
  for (auto &[_, grape] : _grapes) {
    decltype(grape.containers)::accessor acc;
    if (grape.containers.find(acc, containerId)) {
      return grape.id;
    }
  }
  return std::nullopt;
}