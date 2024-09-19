#pragma once
#include <string>

namespace celte {
/**
 * @brief This class is the base class of all entities managed by celte.
 * Engine integrations should include this class as either the base class
 * of their entities or wrap it as a component.
 *
 * It manages the entity's lifecycle as seen by Celte.
 */
class CelteEntity {
public:
  /**
   * @brief Called when the entity is spawned.
   * This is called when the entity is created and added to the scene.
   * It will generate a uuid for the entity and forward it to the chunk
   * that the entity is spawning in.
   *
   * @param x, y, z: The position of the entity in the world, necessary to
   * determine the chunk.
   */
  void OnSpawn(float x, float y, float z);

private:
};
} // namespace celte