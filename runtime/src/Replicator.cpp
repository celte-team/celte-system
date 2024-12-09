#include "Logger.hpp"
#include "Replicator.hpp"
#include "base64.hpp"
#include <cstring>
// #include <msgpack.hpp>
#include "nlohmann/json.hpp"

namespace celte {
namespace runtime {
int Replicator::__computeCheckSum(const std::string &data) {
  int sum = 0;
  for (int i = 0; i < data.size(); i++) {
    sum = 31 * sum + data[i];
  }
  return sum;
}

void Replicator::RegisterReplicatedValue(
    const std::string &name, std::function<std::string()> get,
    std::function<void(const std::string &)> set) {
  _replicatedValues[name] = {get, set, __computeCheckSum(name)};
}

Replicator::ReplBlob Replicator::GetBlob(bool peek) {
  try {
    nlohmann::json j;
    for (auto &[key, value] : _replicatedValues) {
      std::string raw = value.get();
      if (value.hash != __computeCheckSum(raw)) {
        j[key] = raw;
        if (not peek) {
          value.hash = __computeCheckSum(raw);
        }
      }
    }
    return j.dump();
  } catch (std::exception &e) {
    std::cerr << "Error while packing replication data: " << e.what()
              << std::endl;
    return "";
  }
}

void Replicator::Overwrite(const ReplBlob &blob) {
  try {
    nlohmann::json j = nlohmann::json::parse(blob);
    for (auto &[key, value] : j.items()) {
      auto it = _replicatedValues.find(key);
      if (it != _replicatedValues.end()) {
        it->second.set(value.get<std::string>().c_str());
      }
    }
  } catch (std::exception &e) {
    std::cerr << "Error while overwriting replication data: " << e.what()
              << std::endl;
  }
}

} // namespace runtime
} // namespace celte