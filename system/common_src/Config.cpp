#include "Config.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
using namespace celte;

YAML::Node GetYamlRoot() {
  std::string section = "celte";

  // Check for CELTE_CONFIG environment variable
  const char *envPath = std::getenv("CELTE_CONFIG");
  std::string yamlPath;

  if (envPath && std::strlen(envPath) > 0) {
    yamlPath = envPath;
    std::cout << "Using config path from CELTE_CONFIG: " << yamlPath
              << std::endl;
  } else {
    const char *home = std::getenv("HOME");
    if (!home) {
      throw std::runtime_error("HOME environment variable not set");
    }
    yamlPath = std::string(home) + "/.celte.yaml";
    std::cout << "Using default config path: " << yamlPath << std::endl;
  }

  YAML::Node root = YAML::LoadFile(yamlPath);
  if (!root[section]) {
    throw std::runtime_error("Section '" + section + "' not found in YAML");
  }

  return root;
}

Config::Config() {
  try {
    std::string section = "celte";
    std::string yamlPath = std::string(std::getenv("HOME")) + "/.celte.yaml";
    YAML::Node root = YAML::LoadFile(yamlPath);

    for (const auto &entry : root[section]) {
      for (const auto &kv : entry) {
        std::string key = kv.first.as<std::string>();
        std::string value = kv.second.as<std::string>();
        _config[key] = value;
      }
    }

    if (_config.find("CELTE_REDIS_HOST") == _config.end())
      _config["CELTE_REDIS_HOST"] = "localhost";
    if (_config.find("CELTE_REDIS_PORT") == _config.end())
      _config["CELTE_REDIS_PORT"] = "6379";
    if (_config.find("CELTE_REDIS_KEY") == _config.end())
      _config["CELTE_REDIS_KEY"] = "logs";
    if (_config.find("PUSHGATEWAY_HOST") == _config.end())
      _config["PUSHGATEWAY_HOST"] = "localhost";
    if (_config.find("PUSHGATEWAY_PORT") == _config.end())
      _config["PUSHGATEWAY_PORT"] = "9091";
    if (_config.find("METRICS_UPLOAD_INTERVAL") == _config.end())
      _config["METRICS_UPLOAD_INTERVAL"] = "5";
    if (_config.find("REPLICATION_INTERVAL") == _config.end())
      _config["REPLICATION_INTERVAL"] = "1000";

  } catch (const std::exception &e) {
    std::cerr << "Failed to load config from YAML: " << e.what() << std::endl;
    throw;
  }
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

void Config::SetSessionId(const std::string &sessionId) {
  tp::default_scope() = "non-persistent://public/" + sessionId + "/";
  _sessionId = sessionId;
}