#include "CelteEntity.hpp"

namespace celte {
    CelteEntity::CelteEntity(double x, double y, double z)
    {
        __onSpawn(x, y, z);
    }

    CelteEntity::CelteEntity(const std::string& chunkId) { __onSpawn(chunkId); }

    CelteEntity::~CelteEntity() { }

    void CelteEntity::__onSpawn(double x, double y, double z) { }

    void CelteEntity::__onSpawn(const std::string& chunkId) { }
}