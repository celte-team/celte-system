#include "Config.hpp"
#include <laserpants/dotenv/dotenv.h>
using namespace celte;

Config::Config() {
  dotenv::init();

  const char *redis_host = getenv("CELTE_REDIS_HOST");
  _config["redis_host"] = redis_host ? redis_host : "localhost";

  const char *redis_port = getenv("CELTE_REDIS_PORT");
  _config["redis_port"] = redis_port ? redis_port : "6379";

  const char *redis_key = getenv("CELTE_REDIS_KEY");
  _config["redis_key"] = redis_key ? redis_key : "logs";

  const char *pushgateway_host = getenv("CELTE_PULSAR_HOST");
  _config["pushgateway_host"] =
      pushgateway_host ? pushgateway_host : "localhost";

  const char *pushgateway_port = getenv("PUSHGATEWAY_PORT");
  _config["pushgateway_port"] = pushgateway_port ? pushgateway_port : "9091";

  const char *metrics_upload_interval = getenv("METRICS_UPLOAD_INTERVAL");
  _config["metrics_upload_interval"] =
      metrics_upload_interval ? metrics_upload_interval : "5";

  const char *replication_interval = getenv("REPLICATION_INTERVAL");
  _config["replication_interval"] =
      replication_interval ? replication_interval : "1000";
}

std::optional<std::string> Config::Get(const std::string &key) const {
  auto it = _config.find(key);
  if (it == _config.end()) {
    // try to get it from the environment
    const char *env_value = getenv(key.c_str());
    if (env_value) {
      return std::string(env_value);
    }
    // if not found, return nullopt
    return std::nullopt;
  }
  return it->second;
}

void Config::Set(const std::string &key, const std::string &value) {
  _config[key] = value;
}
