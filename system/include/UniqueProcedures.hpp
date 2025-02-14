#pragma once
#define TBB_PREVIEW_CONCURRENT_LRU_CACHE 1
#include <string>
#include <tbb/concurrent_lru_cache.h>

namespace celte {

/// @brief This class is responsible for registering uuids of procedures that
/// must be executed only once but may be triggered by multiple sources.
class UniqueProcedures {
public:
  /// @brief Registers a procedure UUID.
  /// @param uuid The UUID of the procedure.
  /// @return True if the UUID was successfully registered, false if it was
  /// already registered.
  static bool RegisterProcedure(const std::string &uuid);

  /// @brief Checks if a procedure UUID is already registered.
  /// @param uuid The UUID of the procedure.
  /// @return True if the UUID is already registered, false otherwise.
  static bool IsProcedureRegistered(const std::string &uuid);

private:
  struct KeyToValFunctor {
    bool operator()(const std::string &key) const { return true; }
  };
  static tbb::concurrent_lru_cache<std::string, bool, KeyToValFunctor>
      registered_procedures;
};

} // namespace celte