#pragma once
#ifdef CELTE_SERVER_MODE_ENABLED
#include "ClientRegistry.hpp"
#endif
#include "Container.hpp"
#include "Executor.hpp"
#include "RPCService.hpp"
#include <optional>
#include <string>

namespace celte {

struct Grape {
  std::string id;      ///< The unique identifier of the grape.
  bool isLocallyOwned; ///< True if this grape is owned by this peer. (only
                       ///< possible in server mode)
#ifdef CELTE_SERVER_MODE_ENABLED
  std::optional<ClientRegistry> clientRegistry; ///< The client registry for
                                                ///< this grape. Only available
                                                ///< in server mode.
#endif
  Executor executor; ///< The executor for this grape. Tasks to be ran in the
                     ///< engine can be pushed here.
  std::optional<net::RPCService>
      rpcService; ///< The rpc service for this grape. Must be initialized after
  ///< the id has been set.

  tbb::concurrent_hash_map<std::string, Container *>
      containers; ///< All containers that belong to this grape.
};
} // namespace celte