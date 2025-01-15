#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace celte {
/// @brief All the configuration options for the system are accessible from this
/// class.
class Config {
public:
  Config();

  /// @brief Get the value of a configuration option.
  /// @param key The key of the configuration option.
  /// @return The value of the configuration option, if it exists.
  std::optional<std::string> Get(const std::string &key) const;

  /// @brief Set the value of a configuration option.
  /// @param key The key of the configuration option.
  /// @param value The value of the configuration option.
  void Set(const std::string &key, const std::string &value);

private:
  std::unordered_map<std::string, std::string> _config;
};
} // namespace celte