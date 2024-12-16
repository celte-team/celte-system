#pragma once
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <vector>

namespace celte {
namespace chunks {
/**
 * @brief A rotated bounding box is a bounding box that is not aligned
 * with the world's axes, but instead with the axes of an object in the
 * engine.
 */
class RotatedBoundingBox {
public:
  RotatedBoundingBox(const glm::vec3 &position, const glm::vec3 &size,
                     const glm::vec3 &localX, const glm::vec3 &localY,
                     const glm::vec3 &localZ);
  ~RotatedBoundingBox();

  inline float GetDistanceToPosition(float x, float y, float z) const {
    return glm::distance(_position, glm::vec3(x, y, z));
  }

  /**
   * @brief Returns true if the given position is inside the bounding box.
   */
  bool ContainsPosition(float x, float y, float z) const;

  /**
   * @brief Returns a list of points that are equally spaced along the
   * bounding box's axes.
   *
   * @param n: The number of subdivisions along each axis.
   */
  std::vector<glm::vec3> GetMeshedPoints(int subdivision) const;

private:
  const glm::vec3 _position;
  const glm::vec3 _localX;
  const glm::vec3 _localY;
  const glm::vec3 _localZ;
  const glm::vec3 _size;
  glm::vec3 _halfSize;
  glm::vec3 _halfX;
  glm::vec3 _halfY;
  glm::vec3 _halfZ;
};
} // namespace chunks
} // namespace celte