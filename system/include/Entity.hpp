#pragma once
#include "Executor.hpp"
#include "Replicator.hpp"
#include <string>

namespace celte {
    struct Entity {
        std::string id = ""; ///< The entity's unique identifier.
        std::string ownerSN = ""; ///< The entity's owner's serial number.
        std::string ownerContainerId = ""; ///< The entity's owner's container identifier.
        bool quarantine = false; ///< Whether the entity is quarantined (excluded from
                                 ///< network reassignment).
        bool isValid = false; ///< Whether the entity is valid. (if not, it will be
                              ///< ignored by the runtime until it is valid again)
        Executor
            executor; ///< The entity's executor used to push tasks for the entity.

        tbb::concurrent_queue<Replicator::ReplBlob> _replPushed;
    };
} // namespace celte
