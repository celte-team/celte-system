#include "Config.hpp"

using namespace celte;

std::optional<std::string> Config::Get(const std::string &key) const {
  auto it = _config.find(key);
  if (it == _config.end()) {
    return std::nullopt;
  }
  return it->second;
}

void Config::Set(const std::string &key, const std::string &value) {
  _config[key] = value;
}