#include "CelteHooks.hpp"

namespace celte {
    namespace api {
        HooksTable& HooksTable::HOOKS() {
            static HooksTable hooks;
            return hooks;
        }
    } // namespace api
} // namespace celte