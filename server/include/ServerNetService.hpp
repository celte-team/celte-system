#pragma once
#include "CelteNet.hpp"
#include "CelteService.hpp"
#include "RPCService.hpp"
#include "topics.hpp"
#include <stdexcept>

namespace celte {
namespace server {
class CelteServerNetServiceException : public std::runtime_error {
public:
  CelteServerNetServiceException(const std::string &msg)
      : std::runtime_error(msg) {}
};

class CelteServerNetServiceExceptionTimeout
    : public CelteServerNetServiceException {
public:
  CelteServerNetServiceExceptionTimeout()
      : CelteServerNetServiceException(
            "Timeout while connecting to the server") {}
};

class ServerNetService : public net::CelteService {
public:
  void Connect();
  net::RPCService &rpcs() { return *_rpcs; }

  inline void ConnectClientToGrape(std::string clientId, std::string grapeId,
                                   float x, float y, float z) {
    rpcs()
        .CallAsync<bool>(tp::PERSIST_DEFAULT + clientId + "." + tp::RPCs,
                         "__rp_forceConnectToChunk", grapeId, x, y, z)
        .Then([clientId](bool success) {
          if (!success) {
            std::cerr << "Error in ConnectClientToGrape: "
                      << "Client failed to join: " << clientId << std::endl;
          }
        });
  }

  /**
   * @brief Writes a single binary string to a topic. Upon successful delivery
   * (or error) the callback is called synchronously in the main thread.
   */
  void Write(const std::string &topic, const std::string &msg,
             std::function<void(pulsar::Result)> then);

private:
  void __init();
  std::optional<net::RPCService> _rpcs = std::nullopt;
  std::optional<net::WriterStreamPool> _writerStreamPool = std::nullopt;
};
} // namespace server
} // namespace celte