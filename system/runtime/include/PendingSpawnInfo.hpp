#pragma once
#include <glm/vec3.hpp>
#include <string>

namespace celte {
struct PendingSpawnInfo {
  std::string grapeId;
  glm::vec3 position;
};
} // namespace celte