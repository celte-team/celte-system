#include "RotatedBoundingBox.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace celte {
namespace chunks {
RotatedBoundingBox::RotatedBoundingBox(const glm::vec3 &position,
                                       const glm::vec3 &size,
                                       const glm::vec3 &localX,
                                       const glm::vec3 &localY,
                                       const glm::vec3 &localZ)
    : _position(position), _size(size), _localX(localX), _localY(localY),
      _localZ(localZ) {
  _halfSize = _size / 2.0f;
  _halfX = _localX * _halfSize.x;
  _halfY = _localY * _halfSize.y;
  _halfZ = _localZ * _halfSize.z;
}

RotatedBoundingBox::~RotatedBoundingBox() {}

bool RotatedBoundingBox::ContainsPosition(float x, float y, float z) const {
  // looking at the vector localOrigin to position
  glm::vec3 posLocalCoord(glm::vec3(x, y, z) - _position);

  // projecting on local Axis (their norm is one)
  glm::vec3 posLocalCoordProj(glm::dot(posLocalCoord, _localX),
                              glm::dot(posLocalCoord, _localY),
                              glm::dot(posLocalCoord, _localZ));

  // is any of the coords of the projection greater than the half size?
  bool contains = (glm::abs(posLocalCoordProj.x) <= _halfSize.x &&
                   glm::abs(posLocalCoordProj.y) <= _halfSize.y &&
                   glm::abs(posLocalCoordProj.z) <= _halfSize.z);

  return contains;
}

std::vector<glm::vec3>
RotatedBoundingBox::GetMeshedPoints(int subdivision) const {
  std::vector<glm::vec3> points;
  for (int i = 0; i < subdivision; i++) {
    for (int j = 0; j < subdivision; j++) {
      for (int k = 0; k < subdivision; k++) {
        glm::vec3 point = _position + _halfX * (2.0f * i / subdivision - 0.5f) +
                          _halfY * (2.0f * j / subdivision - 0.5f) +
                          _halfZ * (2.0f * k / subdivision - 0.5f);
        points.push_back(point);
      }
    }
  }
  return points;
}
} // namespace chunks
} // namespace celte