#pragma once
#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "RPCService.hpp"
#include <stdexcept>

namespace celte {
namespace client {
class CelteClientNetServiceException : public std::runtime_error {
public:
  CelteClientNetServiceException(const std::string &msg)
      : std::runtime_error(msg) {}
};

class CelteClientNetServiceExceptionTimeout
    : public CelteClientNetServiceException {
public:
  CelteClientNetServiceExceptionTimeout()
      : CelteClientNetServiceException(
            "Timeout while connecting to the server") {}
};

class ClientNetService : public net::CelteService {
public:
  void Connect();
  net::RPCService &rpcs() { return *_rpcs; }

  /**
   * @brief Writes a single binary string to a topic. Upon successful delivery
   * (or error) the callback is called synchronously in the main thread.
   */
  void Write(const std::string &topic, const std::string &msg,
             std::function<void(pulsar::Result)> then);

private:
  std::optional<net::RPCService> _rpcs = std::nullopt;
  std::optional<net::WriterStreamPool> _writerStreamPool = std::nullopt;
};
} // namespace client
} // namespace celte