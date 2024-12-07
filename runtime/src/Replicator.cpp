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
  nlohmann::json j;
  for (auto &[key, value] : _replicatedValues) {
    if (value.hash != __computeCheckSum(value.get())) {
      j[key] = value.get();
      if (not peek) {
        value.hash = __computeCheckSum(value.get());
      }
    }
  }
  return j.dump();
}

void Replicator::Overwrite(const ReplBlob &blob) {
  nlohmann::json j = nlohmann::json::parse(blob);
  for (auto &[key, value] : j.items()) {
    auto it = _replicatedValues.find(key);
    if (it != _replicatedValues.end()) {
      it->second.set(value.get<std::string>());
    }
  }
}

} // namespace runtime
} // namespace celte