#include "RotatedBoundingBox.hpp"
#include <glm/glm.hpp>


namespace celte {
    namespace chunks {
        RotatedBoundingBox::RotatedBoundingBox(const glm::vec3& position, const glm::vec3& size, const glm::vec3& localX, const glm::vec3& localY, const glm::vec3& localZ)
            : _position(position), _size(size), _localX(localX), _localY(localY), _localZ(localZ)
        {
            _halfSize = _size / 2.0f;
            _halfX = _localX * _halfSize.x;
            _halfY = _localY * _halfSize.y;
            _halfZ = _localZ * _halfSize.z;
        }

        RotatedBoundingBox::~RotatedBoundingBox() {}

        bool RotatedBoundingBox::ContainsPosition(float x, float y, float z) const
        {
            glm::vec3 localPosition = glm::vec3(x, y, z) - _position;
            float xDistance = glm::dot(localPosition, _localX);
            float yDistance = glm::dot(localPosition, _localY);
            float zDistance = glm::dot(localPosition, _localZ);

            return (xDistance >= -_halfSize.x && xDistance <= _halfSize.x) &&
                   (yDistance >= -_halfSize.y && yDistance <= _halfSize.y) &&
                   (zDistance >= -_halfSize.z && zDistance <= _halfSize.z);
        }

        std::vector<glm::vec3> RotatedBoundingBox::GetMeshedPoints(int subdivision) const
        {
            std::vector<glm::vec3> points;
            for (int i = 0; i <= subdivision; i++) {
                for (int j = 0; j <= subdivision; j++) {
                    for (int k = 0; k <= subdivision; k++) {
                        glm::vec3 point = _position + _halfX * (2.0f * i / subdivision - 1.0f) +
                                          _halfY * (2.0f * j / subdivision - 1.0f) +
                                          _halfZ * (2.0f * k / subdivision - 1.0f);
                        points.push_back(point);
                    }
                }
            }
            return points;
        }
    } // namespace chunks
} // namespace celte