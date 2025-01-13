#include "ETTRegistry.hpp"

using namespace celte;

ETTRegistry &ETTRegistry::GetInstance() {
  static ETTRegistry instance;
  return instance;
}

void ETTRegistry::RegisterEntity(const Entity &e) {
  accessor acc;
  if (entities.insert(acc, e.id)) {
    acc->second = e;
  } else {
    throw std::runtime_error("Entity with id " + e.id + " already exists.");
  }
}

void ETTRegistry::UnregisterEntity(const std::string &id) {
  accessor acc;
  if (entities.find(acc, id)) {
    if (acc->second.isValid) {
      throw std::runtime_error(
          "Cannot unregister a valid entity as it is still in use.");
    }
    entities.erase(acc);
  }
}