#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace celte {
/// @brief This class is responsible for transferring authority over entities
/// between containers. Containers may not necessarily be owned locally.
class AuthorityTransfer {
public:
  /// @brief This method will send the order on the network to transfer the
  /// authority of the entity to the target container. The rpcs
  /// __rp_containerTakeAuthority and __rp_containerDropAuthority will be called
  /// on the target and source containers respectively.
  /// This will remotely trigger calls to ExecTakeOrder and
  /// ExecDropOrder
  /// @param entityId
  /// @param toContainerId
  static void TransferAuthority(const std::string &entityId,
                                const std::string &toContainerId,
                                const std::string &payload);

  static void ExecTakeOrder(nlohmann::json args);

  static void ExecDropOrder(nlohmann::json args);

private:
};
} // namespace celte