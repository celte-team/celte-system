#include "CelteRequest.hpp"
#include "base64.hpp"
namespace celte {
namespace req {
struct BinaryDataPacket : public celte::net::CelteRequest<BinaryDataPacket> {
  std::string binaryData;
  std::string peerUuid;

  void to_json(nlohmann::json &j) const {
    j = nlohmann::json{{"binaryData", binaryData}, {"peerUuid", peerUuid}};
  }

  void from_json(const nlohmann::json &j) {
    j.at("binaryData").get_to(binaryData);
    j.at("peerUuid").get_to(peerUuid);
  }
};

struct SpawnPositionRequest
    : public celte::net::CelteRequest<SpawnPositionRequest> {
  std::string clientId;
  std::string grapeId;
  float x;
  float y;
  float z;

  void to_json(nlohmann::json &j) const {
    j = nlohmann::json{{"clientId", clientId},
                       {"grapeId", grapeId},
                       {"x", x},
                       {"y", y},
                       {"z", z}};
  }

  void from_json(const nlohmann::json &j) {
    j.at("clientId").get_to(clientId);
    j.at("grapeId").get_to(grapeId);
    j.at("x").get_to(x);
    j.at("y").get_to(y);
    j.at("z").get_to(z);
  }
};

struct ReplicationDataPacket
    : public celte::net::CelteRequest<ReplicationDataPacket> {
  std::unordered_map<std::string, std::string> data; // entity id -> blob
  bool active;

  void to_json(nlohmann::json &j) const {
    std::unordered_map<std::string, std::string> encoded_data;
    for (const auto &pair : data) {
      encoded_data[pair.first] = base64_encode(
          reinterpret_cast<const unsigned char *>(pair.second.data()),
          pair.second.size());
    }
    j = nlohmann::json{{"data", encoded_data}, {"active", active}};
  }

  void from_json(const nlohmann::json &j) {
    std::unordered_map<std::string, std::string> encoded_data;
    j.at("data").get_to(encoded_data);
    for (const auto &pair : encoded_data) {
      data[pair.first] = base64_decode(pair.second);
    }
    j.at("active").get_to(active);
  }
};

} // namespace req
} // namespace celte