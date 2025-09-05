#pragma once
#include <array>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace celte {
class KMeans {
public:
  struct Cluster; // Forward declaration
  struct Point {
    glm::vec3 position;
    std::string clusterId; // ID of the cluster this point belongs to
    std::string id;
  };

  struct Cluster {
    glm::vec3 centroid;
  };

  struct ChangeData {
    std::string pointId;
    std::string oldClusterId;
    std::string newClusterId;
  };

  using Index = size_t;

public:
  KMeans(int maxIterations = 10, int maxPointsPerCluster = 10);
  ~KMeans() = default;

  /// @warning Not thread-safe, must be called at a safe time.
  void AddPoint(const glm::vec3 &point, const std::string &id);

  void UpdatePoint(const glm::vec3 &newPos, const std::string &id);

  /// @warning Not thread-safe, must be called at a safe time.
  void AddCluster(const glm::vec3 &centroid, const std::string &id);

  /// @warning Not thread-safe, must be called at a safe time.
  void RemovePoint(const std::string &id);

  /// @warning Not thread-safe, must be called at a safe time.
  void RemoveCluster(const std::string &id);

  std::vector<ChangeData> Iterate();

private:
  int _maxIterations;
  int _maxPointsPerCluster;
  std::unordered_map<std::string, Point> _points;
  std::unordered_map<std::string, Cluster> _clusters;
};
} // namespace celte
