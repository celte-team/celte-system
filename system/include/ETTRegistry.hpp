#pragma once
#include "Entity.hpp"
#include <string>
#include <tbb/concurrent_hash_map.h>

#define ETTREGISTRY celte::ETTRegistry::GetInstance()

namespace celte {
class ETTRegistry {
public:
  using accessor = tbb::concurrent_hash_map<std::string, Entity>::accessor;
  static ETTRegistry &GetInstance();

  /// @brief Registers an entity in the registry.
  /// @param e The entity to register.
  void RegisterEntity(const Entity &e);

  /// @brief Unregisters an entity from the registry.
  /// @param id The entity's unique identifier.
  void UnregisterEntity(const std::string &id);

private:
  tbb::concurrent_hash_map<std::string, Entity> entities;
};
} // namespace celte
