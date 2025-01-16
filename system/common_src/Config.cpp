#include "Config.hpp"

using namespace celte;

Config::Config() { // Set default values
  const char *redis_host = std::getenv("REDIS_HOST");
  _config["redis_host"] = redis_host ? redis_host : "localhost";

  const char *redis_port = std::getenv("REDIS_PORT");
  _config["redis_port"] = redis_port ? redis_port : "6379";

  const char *redis_key = std::getenv("REDIS_KEY");
  _config["redis_key"] = redis_key ? redis_key : "logs";
}

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