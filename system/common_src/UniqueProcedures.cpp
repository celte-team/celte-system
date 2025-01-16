/*
** Authors: Celte Team (Eliot Janvier, Laurent Jiang, Ewen Brian, Thomas Laprie)
** Date: 1/16/2025
** File: /system/common_src/UniqueProcedures.cpp
** Copyright: Celte Team
*/

#include "UniqueProcedures.hpp"

using namespace celte;

tbb::concurrent_lru_cache<std::string, bool, UniqueProcedures::KeyToValFunctor>
    UniqueProcedures::registered_procedures(
        KeyToValFunctor(), 1000); // Set the cache size limit as needed

bool UniqueProcedures::RegisterProcedure(const std::string &uuid) {
  auto handle = registered_procedures[uuid];
  return handle.value(); // true if the UUID was successfully inserted, false if
                         // it was already present
}

bool UniqueProcedures::IsProcedureRegistered(const std::string &uuid) {
  auto handle = registered_procedures[uuid];
  return handle.value();
}
