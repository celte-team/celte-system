#include "HttpClient.hpp"
#include "Runtime.hpp"
#include <iostream>

namespace celte {

HttpClient::HttpClient(ErrorHandler errorHandler)
    : errorHandler(std::move(errorHandler)) {}

void HttpClient::PostAsync(const std::string &url, nlohmann::json jsonBody,
                           Callback callback) {
  RUNTIME.ScheduleAsyncIOTask([this, url, jsonBody, callback]() {
    auto response =
        cpr::Post(cpr::Url{url}, cpr::Body{jsonBody.dump()},
                  cpr::Header{{"Content-Type", "application/json"}});
    __handleResponse(response, std::move(callback));
  });
}

std::string HttpClient::Post(const std::string &url, nlohmann::json jsonBody) {
  auto response = cpr::Post(cpr::Url{url}, cpr::Body{jsonBody.dump()},
                            cpr::Header{{"Content-Type", "application/json"}});
  return response.text;
}

void HttpClient::__handleResponse(const cpr::Response &response,
                                  Callback callback) {
  if (response.status_code >= 200 && response.status_code < 300) {
    __parseAndHandleJson(response.text, std::move(callback));
  } else {
    __handleError(response.status_code, response.error.message);
  }
}

void HttpClient::__parseAndHandleJson(const std::string &responseText,
                                      Callback callback) {
  try {
    auto jsonResponse = nlohmann::json::parse(responseText);
    callback(jsonResponse);
  } catch (const std::exception &e) {
    __handleError(500, e.what());
  }
}

void HttpClient::__handleError(int statusCode, const std::string &message) {
  if (errorHandler) {
    errorHandler(statusCode, message);
  } else {
    std::cerr << "Error [" << statusCode << "]: " << message << std::endl;
  }
}

} // namespace celte
