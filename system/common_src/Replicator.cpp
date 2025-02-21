/*
** CELTE, 2025
** celte-system

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** Replicator
*/
#include "Replicator.hpp"
#include "nlohmann/json.hpp"
#include <cstring>

using namespace celte;

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
    bool changed = false;
    for (auto &[key, value] : _replicatedValues) {
      std::string raw = value.get();
      int currChecksum = __computeCheckSum(raw);
      if (value.hash != currChecksum or peek) {
        changed = true;
        j[key] = raw;
        if (not peek) {
          value.hash = currChecksum;
        }
      }
    }
    if (changed or peek) {
      return j.dump();
    }
    return "";
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
      } else {
        std::cerr << "Key " << key << " not found in replicator" << std::endl;
      }
    }
  } catch (std::exception &e) {
    std::cerr << "Error while overwriting replication data: " << e.what()
              << std::endl;
  }
}
