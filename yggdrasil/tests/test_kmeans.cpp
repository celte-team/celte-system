#include "KMeans.hpp"
#include <gtest/gtest.h>

using namespace celte;

TEST(KMeansEdgeCases, NoClusters) {
  KMeans km;
  km.AddPoint(glm::vec3(1.0f, 2.0f, 3.0f), "p0");
  // With no clusters, Iterate should be a no-op and return empty changes
  auto changes = km.Iterate();
  EXPECT_TRUE(changes.empty());
}

TEST(KMeansBasic, SingleClusterStability) {
  KMeans km;
  km.AddCluster(glm::vec3(0.0f, 0.0f, 0.0f), "c0");

  km.AddPoint(glm::vec3(0.1f, 0.0f, 0.0f), "p0");
  km.AddPoint(glm::vec3(0.2f, 0.0f, 0.0f), "p1");

  auto changes = km.Iterate();
  // both points should be initially assigned to the single cluster
  EXPECT_EQ(changes.size(), 2);

  // Next iteration should not change assignments (stable)
  auto changes2 = km.Iterate();
  EXPECT_EQ(changes2.size(), 0);
}

TEST(KMeansDynamicClusters, RemoveClusterReassignsPoints) {
  KMeans km;
  km.AddCluster(glm::vec3(0.0f, 0.0f, 0.0f), "c0");
  km.AddCluster(glm::vec3(10.0f, 0.0f, 0.0f), "c1");

  km.AddPoint(glm::vec3(0.1f, 0.0f, 0.0f), "p0");
  km.AddPoint(glm::vec3(9.9f, 0.0f, 0.0f), "p1");
  km.AddPoint(glm::vec3(10.1f, 0.0f, 0.0f), "p2");

  auto initial = km.Iterate();
  EXPECT_EQ(initial.size(), 3);

  // Remove the cluster that owned p1 and p2 (likely c1)
  km.RemoveCluster("c1");

  auto afterRemoval = km.Iterate();
  // Points that were in c1 should now be reassigned (at least one change)
  EXPECT_GE(afterRemoval.size(), 1);

  // Ensure no point remains assigned to a removed cluster by checking
  // that no change entry references a newClusterId equal to an erased id
  for (const auto &c : afterRemoval) {
    EXPECT_NE(c.newClusterId, "c1");
  }
}

TEST(KMeansUpdatePoint, UpdateCausesReassignment) {
  KMeans km;
  km.AddCluster(glm::vec3(-5.0f, 0.0f, 0.0f), "left");
  km.AddCluster(glm::vec3(5.0f, 0.0f, 0.0f), "right");

  km.AddPoint(glm::vec3(-4.9f, 0.0f, 0.0f), "mover");

  auto init = km.Iterate();
  EXPECT_EQ(init.size(), 1);

  // Move the point across the decision boundary to the right cluster
  km.UpdatePoint(glm::vec3(6.0f, 0.0f, 0.0f), "mover");
  auto changes = km.Iterate();
  // The moved point should be reassigned
  EXPECT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].pointId, "mover");
}

TEST(KMeansDynamicAddCluster, AddClusterAfterPoints) {
  KMeans km;
  km.AddPoint(glm::vec3(0.0f, 0.0f, 0.0f), "p0");
  km.AddPoint(glm::vec3(100.0f, 0.0f, 0.0f), "p1");

  // Add clusters after points exist
  km.AddCluster(glm::vec3(0.0f, 0.0f, 0.0f), "c0");
  km.AddCluster(glm::vec3(100.0f, 0.0f, 0.0f), "c1");

  auto changes = km.Iterate();
  EXPECT_EQ(changes.size(), 2);
}
