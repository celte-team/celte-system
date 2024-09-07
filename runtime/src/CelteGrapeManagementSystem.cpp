#include "CelteGrapeManagementSystem.hpp"
#include <stdexcept>

namespace celte {
    namespace chunks {
        Grape& CelteGrapeManagementSystem::RegisterGrape(const GrapeOptions& options)
        {
            auto grape = std::make_shared<Grape>(options);
            _grapes[grape->GetGrapeId()] = grape;
            return *grape;
        }

        CelteGrapeManagementSystem& CelteGrapeManagementSystem::GRAPE_MANAGER()
        {
            static CelteGrapeManagementSystem instance;
            return instance;
        }

        Grape& CelteGrapeManagementSystem::GetGrape(std::string grapeId)
        {
            if (_grapes.find(grapeId) == _grapes.end()) {
                throw std::out_of_range("Grape with id " + grapeId + " does not exist.");
            }
            return *_grapes[grapeId];
        }

        Grape& CelteGrapeManagementSystem::GetGrapeByPosition(float x, float y, float z)
        {
            for (auto& [grapeId, grape] : _grapes) {
                if (grape->ContainsPosition(x, y, z)) {
                    return *grape;
                }
            }
            throw std::out_of_range("No grape contains the position (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ").");
        }
    } // namespace chunks
} // namespace celte