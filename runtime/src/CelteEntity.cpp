#include "CelteEntity.hpp"
#include "CelteGrapeManagementSystem.hpp"
#include "CelteHooks.hpp"
#include <iostream>

namespace celte {
    void CelteEntity::OnSpawn(float x, float y, float z)
    {
        try {
        auto chunk = chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()
            .GetGrapeByPosition(x, y, z)
            .GetChunkByPosition(x, y, z);
        chunk.OnEntitySpawn(*this);
        } catch (std::out_of_range &e) {
            // Entity is not in any grape
            std::cerr << "Entity is not in any grape: " << e.what() << std::endl;
        }
    }