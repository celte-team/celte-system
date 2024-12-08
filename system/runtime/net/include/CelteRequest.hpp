#pragma once
#include "nlohmann/json.hpp"

namespace celte {
namespace net {

template <typename CRTP> struct CelteRequest {
  std::map<std::string, std::string> headers;

  friend void to_json(nlohmann::json &j, const CelteRequest &req) {
    static_cast<const CRTP &>(req).to_json(j);
  }

  friend void from_json(const nlohmann::json &j, CelteRequest &req) {
    static_cast<CRTP &>(req).from_json(j);
  }
};

} // namespace net
} // namespace celte
