#include "KMeans.hpp"
#include <limits>

namespace celte {

KMeans::KMeans(int maxIterations, int maxPointsPerCluster)
    : _maxIterations(maxIterations), _maxPointsPerCluster(maxPointsPerCluster) {
}

void KMeans::AddPoint(const glm::vec3 &point, const std::string &id) {
  _points.emplace(id, Point{point, "", id});
}

void KMeans::UpdatePoint(const glm::vec3 &newPos, const std::string &id) {
  auto it = _points.find(id);
  if (it != _points.end()) {
    it->second.position = newPos;
  }
}

void KMeans::AddCluster(const glm::vec3 &centroid, const std::string &id) {
  if (_clusters.find(id) != _clusters.end()) {
    throw std::runtime_error("Cluster with id " + id + " already exists.");
  }
  _clusters[id] = Cluster{centroid};
  _clusters[id] = Cluster{centroid};
}

void KMeans::RemovePoint(const std::string &id) { _points.erase(id); }

void KMeans::RemoveCluster(const std::string &id) {
  auto it = _clusters.find(id);
  if (it != _clusters.end()) {
    _clusters.erase(it);
  }
}

std::vector<KMeans::ChangeData> KMeans::Iterate() {
  std::vector<ChangeData> changes;

  // If there are no clusters, nothing to assign; return empty changes
  if (_clusters.empty()) {
    return changes;
  }

  // Map cluster id -> index
  std::vector<std::string> clusterIds;
  clusterIds.reserve(_clusters.size());
  for (auto &kv : _clusters) {
    clusterIds.push_back(kv.first);
  }

  const int K = static_cast<int>(clusterIds.size());

  // Prepare accumulators
  std::vector<glm::vec3> sums(K, glm::vec3(0.0f));
  std::vector<int> counts(K, 0);

  // determine source and destination buffer indexes
  // We'll write new assignments into a temporary vector and then move it
  std::unordered_map<std::string, Point> dstPoints;
  dstPoints.reserve(_points.size());

  // For each point in source buffer, find nearest cluster and write to dst
  for (const auto &kv : _points) {
    const Point &p = kv.second;
    // If the point's stored clusterId references a removed cluster, treat it as
    // empty Find nearest
    float bestDist = std::numeric_limits<float>::max();
    int bestIdx = -1;
    for (int i = 0; i < K; ++i) {
      const glm::vec3 &c = _clusters.at(clusterIds[i]).centroid;
      glm::vec3 diff = p.position - c;
      float d = glm::dot(diff, diff);
      if (d < bestDist) {
        bestDist = d;
        bestIdx = i;
      }
    }

    if (bestIdx < 0) {
      // should not happen, but skip safely
      continue;
    }

    Point newP = p;
    std::string newClusterId = clusterIds[bestIdx];
    newP.clusterId = newClusterId;
    dstPoints.emplace(newP.id, std::move(newP));

    // accumulate
    sums[bestIdx] += newP.position;
    counts[bestIdx]++;

    // record change if clusterId was different
    std::string oldId = p.clusterId;
    if (oldId != newClusterId) {
      changes.push_back(ChangeData{p.id, oldId, newClusterId});
    }
  }

  // Update centroids; handle empty clusters by leaving centroid unchanged
  for (int i = 0; i < K; ++i) {
    if (counts[i] > 0) {
      _clusters[clusterIds[i]].centroid =
          sums[i] / static_cast<float>(counts[i]);
    }
  }

  // Replace points with new assignments (map move)
  _points = std::move(dstPoints);

  return changes;
}

} // namespace celte
