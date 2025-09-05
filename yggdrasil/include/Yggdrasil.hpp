#pragma once
#include "KMeans.hpp"
#include <memory>
#include <mutex>
#include <pulsar/Client.h>
#include <queue>
#include <unordered_map>

namespace celte {
class Yggdrasil {
public:
  Yggdrasil(const std::string &pulsarHost, const std::string &sessionId,
            int kmeansMaxIterations = 10, int kmeansMaxPointsPerCluster = 10);
  ~Yggdrasil();

  /// @brief Initializes the Yggdrasil system, connecting to Pulsar and setting
  /// up necessary resources.
  void __init(const std::string &pulsarHost);

  /// @brief Applies the updates queued from the network to the local state.
  void ApplyUpdate();

  /// @brief Interates the KMeans algorithm, updating clusters and point
  /// assignments. Broadcasts changes over the network.
  void KMeansIterate();

private:
  std::unique_ptr<pulsar::Client> _client;

  // --- recv resources ---

  void __initPosUpdateConsumer();
  pulsar::Consumer _posUpdateConsumer;

  void __handlePosUpdate(const pulsar::Message &msg);
  struct PositionUpdate {
    std::string pointId;
    glm::vec3 newPosition;
  };
  std::queue<PositionUpdate> _updateQueue;
  std::mutex _updateMutex;

  // --- send resources ---

  void __createProducer(const std::string &topic);
  void __bufferAuthorityTransferOrder(const KMeans::ChangeData &change);
  void __batchSendAuthorityTransferOrders();

  // Helper methods used by __batchSendAuthorityTransferOrders
  std::unordered_map<std::string, std::vector<KMeans::ChangeData>>
  __drainAuthorityTransferBuffers();
  bool __ensureProducerForTopic(const std::string &topic);
  std::string __serializeAuthorityTransferBatch(
      const std::vector<KMeans::ChangeData> &changes);
  bool __sendSerializedMessageToTopic(const std::string &topic,
                                      const std::string &serialized);
  std::unordered_map<std::string, std::vector<KMeans::ChangeData>>
      _authorityTransferBuffers;
  std::mutex _authorityTransferBuffersMutex;
  std::unordered_map<std::string, pulsar::Producer> _producers;
  std::mutex _producersMutex;

  // --- common resources ---

  std::string _sessionId;
  KMeans _kmeans;
};
} // namespace celte