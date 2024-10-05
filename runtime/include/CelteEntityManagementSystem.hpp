#pragma once
#include "CelteEntity.hpp"
#include <unordered_map>

namespace celte {
namespace runtime {
class CelteEntityManagementSystem {
public:
  /**
   * @brief Registers an entity with the system, enabling quick access to it
   * until it is unregistered.
   *
   * This method should only be called by the CelteEntity class OnSpawn method.
   */
  void RegisterEntity(const celte::CelteEntity &entity);

  /**
   * @brief Unregisters an entity from the system.
   *
   * This method should only be called by the CelteEntity class OnDestroy
   * method.
   */
  void UnregisterEntity(const celte::CelteEntity &entity);

  /**
   * @brief Returns a reference to the entity with the given uuid.
   *
   * @throws std::out_of_range if the entity is not found.
   */
  celte::CelteEntity &GetEntity(const std::string &uuid) const;

private:
  std::unordered_map<std::string, celte::CelteEntity *> _entities;
};
} // namespace runtime
} // namespace celte