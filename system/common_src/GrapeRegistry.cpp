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
  if (_grapes.insert(acc, grapeId)) {
    acc->second.id = grapeId;
    acc->second.isLocallyOwned = isLocallyOwned;
#ifdef CELTE_SERVER_MODE_ENABLED
    if (isLocallyOwned) {
      acc->second.clientRegistry.emplace(); // create the client registry
      acc->second.clientRegistry->StartKeepAliveThread();
    }
#endif
    if (onReady) {
      RUNTIME.ScheduleAsyncTask([onReady, grapeId]() {
        // wait for the rpc service to be ready
        accessor acc2;
        if (GRAPES.GetGrapes().find(acc2, grapeId)) {
          while (!acc2->second.rpcService.has_value() and
                 not acc2->second.rpcService->Ready()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }
        }
        onReady();
      });
    }
  } else {
    throw std::runtime_error("Grape with id " + grapeId + " already exists.");
  }
}

void GrapeRegistry::UnregisterGrape(const std::string &grapeId) {
  accessor acc;
  if (_grapes.find(acc, grapeId)) {
    _grapes.erase(acc);
  }
}