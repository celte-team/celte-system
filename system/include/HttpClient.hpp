#pragma once

#include <cpr/cpr.h>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace celte {

class HttpClient {
public:
  using Callback = std::function<void(const nlohmann::json &)>;
  using ErrorHandler = std::function<void(int, std::string)>;

  HttpClient(ErrorHandler errorHandler);
  void PostAsync(const std::string &url, nlohmann::json jsonBody,
                 Callback callback);
  std::string Post(const std::string &url, nlohmann::json jsonBody);

private:
  void __handleResponse(const cpr::Response &response, Callback callback);
  void __parseAndHandleJson(const std::string &responseText, Callback callback);
  void __handleError(int statusCode, const std::string &message);

  ErrorHandler errorHandler;
};

} // namespace celte