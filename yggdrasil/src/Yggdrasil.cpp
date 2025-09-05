#include "Yggdrasil.hpp"
#include "systems_structs.pb.h"
#include <thread>

namespace celte {
Yggdrasil::Yggdrasil(const std::string &pulsarHost,
                     const std::string &sessionId, int kmeansMaxIterations,
                     int kmeansMaxPointsPerCluster)
    : _client(nullptr), _sessionId(sessionId),
      _kmeans(kmeansMaxIterations, kmeansMaxPointsPerCluster) {
  __init(pulsarHost);
  __initPosUpdateConsumer();
}

Yggdrasil::~Yggdrasil() {
  _posUpdateConsumer.close();
  _client->close();
}

void Yggdrasil::__init(const std::string &pulsarHost) {
  // We connect the pulsar client to the cluster
  pulsar::ClientConfiguration conf;
  conf.setOperationTimeoutSeconds(2);

  conf.setIOThreads(5);
  conf.setMessageListenerThreads(1);
  conf.setUseTls(false);
  conf.setLogger(new pulsar::ConsoleLoggerFactory(pulsar::Logger::LEVEL_WARN));

  std::string pulsarBrokers = pulsarHost;
  _client = std::make_unique<pulsar::Client>(pulsarBrokers, conf);
}

void Yggdrasil::__initPosUpdateConsumer() {
  pulsar::ConsumerConfiguration consumerConf;
  consumerConf.setConsumerType(pulsar::ConsumerShared);
  consumerConf.setMessageListener(
      [this](pulsar::Consumer consumer, pulsar::Message msg) {
        __handlePosUpdate(msg);
        consumer.acknowledge(msg);
      });

  std::string topic = "persistent://public/default/position-updates";
  std::string subscriptionName =
      _sessionId + ".ypl"; // yggdrasil position listener : ypl

  pulsar::Result result = _client->subscribe(topic, subscriptionName,
                                             consumerConf, _posUpdateConsumer);
  if (result != pulsar::ResultOk) {
    throw std::runtime_error("Failed to subscribe to position updates topic");
  }
}

void Yggdrasil::__handlePosUpdate(const pulsar::Message &msg) {
  // data is sent as binary glm::vec3, followed by pointId string
  if (msg.getLength() < sizeof(glm::vec3) + 1) {
    // Message too short to contain a valid update
    return;
  }
  const char *data = static_cast<const char *>(msg.getData());
  glm::vec3 newPosition;
  std::memcpy(&newPosition, data, sizeof(glm::vec3));
  std::string pointId(data + sizeof(glm::vec3),
                      msg.getLength() - sizeof(glm::vec3));
  {
    std::lock_guard<std::mutex> lock(_updateMutex);
    _updateQueue.push({pointId, newPosition});
  }
}

void Yggdrasil::ApplyUpdate() {
  // Apply network updates
  std::queue<PositionUpdate> updates;
  {
    std::lock_guard<std::mutex> lock(_updateMutex);
    std::swap(updates, _updateQueue);
  }
  while (!updates.empty()) {
    const auto &update = updates.front();
    _kmeans.UpdatePoint(update.newPosition, update.pointId);
    updates.pop();
  }
}

void Yggdrasil::KMeansIterate() {
  // Iterate KMeans algorithm
  auto changes = _kmeans.Iterate();
  for (const auto &change : changes) {
    __bufferAuthorityTransferOrder(change);
  }
}

void Yggdrasil::__createProducer(const std::string &topic) {
  std::lock_guard<std::mutex> lock(_producersMutex);
  if (_producers.find(topic) != _producers.end()) {
    return; // Producer already exists
  }

  pulsar::ProducerConfiguration producerConf;
  producerConf.setBatchingEnabled(true);
  producerConf.setBatchingMaxMessages(100);
  producerConf.setBatchingMaxPublishDelayMs(10);
  producerConf.setCompressionType(pulsar::CompressionLZ4);

  pulsar::Producer producer;
  pulsar::Result result =
      _client->createProducer(topic, producerConf, producer);
  if (result != pulsar::ResultOk) {
    throw std::runtime_error("Failed to create producer for topic " + topic);
  }
  _producers[topic] = std::move(producer);
}

void Yggdrasil::__bufferAuthorityTransferOrder(
    const KMeans::ChangeData &change) {
  std::lock_guard<std::mutex> lock(_authorityTransferBuffersMutex);
  _authorityTransferBuffers[change.newClusterId].push_back(change);
}

void Yggdrasil::__batchSendAuthorityTransferOrders() {
  auto buffers = __drainAuthorityTransferBuffers();

  for (const auto &[clusterId, changes] : buffers) {
    if (changes.empty()) {
      continue;
    }

    std::string topic =
        "non-persistent://public/" + _sessionId + "/" + clusterId;

    if (!__ensureProducerForTopic(topic)) {
      // Failed to ensure a producer for this topic, skip
      continue;
    }

    std::string serialized = __serializeAuthorityTransferBatch(changes);
    if (serialized.empty()) {
      continue;
    }

    if (!__sendSerializedMessageToTopic(topic, serialized)) {
      // send failed; currently no retry logic
    }
  }
}

std::unordered_map<std::string, std::vector<KMeans::ChangeData>>
Yggdrasil::__drainAuthorityTransferBuffers() {
  std::unordered_map<std::string, std::vector<KMeans::ChangeData>> buffers;
  {
    std::lock_guard<std::mutex> lock(_authorityTransferBuffersMutex);
    std::swap(buffers, _authorityTransferBuffers);
  }
  return buffers;
}

bool Yggdrasil::__ensureProducerForTopic(const std::string &topic) {
  try {
    __createProducer(topic);
    return true;
  } catch (const std::exception &e) {
    // Failed to create producer
    return false;
  }
}

std::string Yggdrasil::__serializeAuthorityTransferBatch(
    const std::vector<KMeans::ChangeData> &changes) {
  if (changes.empty()) {
    return {};
  }
  celte::req::RPRequest rp;
  std::string body;
  for (const auto &change : changes) {
    body += change.pointId + "," + change.oldClusterId + "," +
            change.newClusterId + "\n";
  }
  rp.set_name("authority_transfer_batch");
  rp.set_args(body);
  std::string serialized;
  if (!rp.SerializeToString(&serialized)) {
    return {};
  }
  return serialized;
}

bool Yggdrasil::__sendSerializedMessageToTopic(const std::string &topic,
                                               const std::string &serialized) {
  pulsar::Message msg = pulsar::MessageBuilder()
                            .setContent(serialized)
                            .setProperty("sessionId", _sessionId)
                            .build();

  std::lock_guard<std::mutex> lock(_producersMutex);
  auto it = _producers.find(topic);
  if (it == _producers.end()) {
    return false;
  }
  pulsar::Result result = it->second.send(msg);
  return result == pulsar::ResultOk;
}

} // namespace celte