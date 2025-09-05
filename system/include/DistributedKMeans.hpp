#pragma once

namespace celte {
class DistributedKMeans {
public:
  /// @brief Registers this cluster to redis.
  void RegisterCluster();
};
} // namespace celte