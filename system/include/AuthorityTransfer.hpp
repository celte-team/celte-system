#pragma once
#include "CelteError.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace celte {
/// @brief Exception type related to authority transfer.
class AuthorityTransferException : public CelteError {
public:
  AuthorityTransferException(const std::string &msg, RedisDb &log,
                             std::string file, int line)
      : CelteError(msg, log, file, line) {}
};

/// @brief This class is responsible for transferring authority over entities
/// between containers. Containers may not necessarily be owned locally.
class AuthorityTransfer {
public:
  /// @brief This method will send the order on the network to transfer the
  /// authority of the entity to the target container. The rpcs
  /// ContainerTakeAuthority and ContainerDropAuthority will be called
  /// on the target and source containers respectively.
  /// This will remotely trigger calls to ExecTakeOrder and
  /// ExecDropOrder
  /// @param entityId
  /// @param toContainerId
  static void TransferAuthority(const std::string &entityId,
                                const std::string &toContainerId,
                                const std::string &payload,
                                bool ignoreNoMove = false);

  static void ExecTakeOrder(nlohmann::json args);

  static void ExecDropOrder(nlohmann::json args);

#ifdef CELTE_SERVER_MODE_ENABLED
  static void ProxyTakeAuthority(const std::string &grapeId,
                                 const std::string &entityId,
                                 const std::string &fromContainerId,
                                 const std::string &payload);

  static void __rp_proxyTakeAuthority(const std::string &newOwnerGrapeId,
                                      const std::string &entityId,
                                      const std::string &fromContainerId,
                                      const std::string &payload);
#endif

private:
};
} // namespace celte