#pragma once
#include <boost/json.hpp>
#include <functional>
#include <kafka/KafkaConsumer.h>
#include <string>
#include <type_traits>

namespace celte {
namespace api {

/**
 * @brief To allow maximum customization of celte's behaviors, most actions
 * taken by celte can be coupled with user defined hooks.
 *
 * pre_cev should return a boolean which, if false, will prevent the event cev
 * from being processed and abord the procedure.
 */
class HooksTable {
public:
#ifdef CELTE_SERVER_MODE_ENABLED
  struct {
    struct {
      /**
       * @brief This hook is called when the server starts trying to reach for
       * the kafka cluster.
       */
      std::function<bool()> onConnectionProcedureInitiated = []() {
        return true;
      };

      /**
       * @brief This hook is called when the server successfully connects to the
       * kafka cluster.
       */
      std::function<bool()> onConnectionSuccess = []() { return true; };

      /**
       * @brief This hook is called when the server fails to connect to the
       * kafka cluster.
       */
      std::function<bool()> onConnectionError = []() { return true; };

      /**
       * @brief This hook is called when the server is disconnected from the
       * kafka cluster for any reason.
       */
      std::function<bool()> onServerDisconnected = []() { return true; };

      /**
       * @brief This hook is called when the master receives a new connection
       * and must decide where to redirect the player. The game dev is expected
       * to look up the player's last known position and data, and return it so
       * that it can be forwarded to the server node.
       *
       * The hook is called by the impolementation of
       * celte::server::states::Connected::__rp_getPlayerSpawnPosition.
       *
       * @return A tuple containing the grape id, the x, y, z coordinates of the
       * spawn for the client that has been requested
       */
      std::function<std::tuple<std::string, std::string, float, float, float>(
          std::string)>
          onSpawnPositionRequest = [](std::string clientId) {
            return std::make_tuple(
                "hook onSpawnPositionRequest not implemented", "null", 0, 0, 0);
          };
    } connection;

    struct {
      /**
       * @brief This hook is called when a new player connects to the server.
       */
      std::function<bool(std::string)> accept = [](std::string clientId) {
        return true;
      };

      /**
       * @brief This hook is called when a new player connects to the server,
       * and must be instantiated in the game world.
       */
      std::function<bool(std::string, int, int, int)> execPlayerSpawn =
          [](std::string clientId, int x, int y, int z) { return true; };
    } newPlayerConnected;

    /**
     * @brief This collection of hooks regroups all hooks related to
     * managing the lifecycle of a grape, and instantiating it into the game
     * world.
     */
    struct {
      /**
       * @brief This hook is called when the server is informed that a new grape
       * has been assigned to it.
       */
      std::function<bool(std::string, bool)> loadGrape =
          [](std::string grapeId, bool isLocallyOwned) { return true; };
    } grape;

    /**
     * @brief this collection of hooks regroups all hooks related to managing
     * the passing of authority over entities.
     */
    struct {
      /**
       * @brief This hook is called when the server is informed that an entity
       * has been assigned to a new chunk.
       */
      std::function<void(std::string, std::string chunkId)> onTake =
          [](std::string entityId, std::string chunkId) {};
    } authority;

    struct {
      /**
       * @brief This hook is called when the server receives replication data.
       *
       * @param entityId The id of the entity that the data is related to.
       * @param dataBlob The data that has been received.
       */
      std::function<void(std::string, std::string)> onReplicationDataReceived =
          [](std::string entityId, std::string dataBlob) {};

      /**
       * @brief This hook is called when the server receives active replication
       * data.
       *
       * @param entityId The id of the entity that the data is related to.
       * @param dataBlob The data that has been received.
       */
      std::function<void(std::string, std::string)>
          onActiveReplicationDataReceived =
              [](std::string entityId, std::string dataBlob) {};
    } replication;
  } server;

#else
  struct {
    struct {
      /**
       * @brief This hook is called when the Client starts trying to reach for
       * the kafka cluster.
       */
      std::function<bool()> onConnectionProcedureInitiated = []() {
        return true;
      };

      /**
       * @brief This hook is called when the Client successfully connects to
       * the kafka cluster.
       */
      std::function<bool()> onConnectionSuccess = []() { return true; };

      /**
       * @brief This hook is called when the Client fails to connect to the
       * kafka cluster.
       */
      std::function<bool()> onConnectionError = []() { return true; };

      /**
       * @brief This hook is called when the Client is disconnected from the
       * kafka cluster for any reason.
       */
      std::function<bool()> onClientDisconnected = []() { return true; };

      /**
       * @brief This hook is called when the server has informed the client of
       * which chunk it should be spawning in, and the client has already
       * subscribed to all the required topics of this chunk. The client is now
       * able to spawn and should request the server to do so using
       * RUNTIME.RequestSpawn(clientId) when it is ready.
       *
       * @note The game dev does not have to call RequestSpawn right away, but
       * this hook being called indicates that everything is ready for him / her
       * to do so.
       *
       */
      std::function<bool(const std::string &grapeId, float x, float y, float z)>
          onReadyToSpawn = [](const std::string &grapeId, float x, float y,
                              float z) { return true; };
    } connection;

    struct {
      /**
       * @brief This hook is called when the client is authorized to spawn a new
       * entity. It should implement the logic to spawn the entity in the game.
       */
      std::function<bool(std::string, int x, int y, int z)> execPlayerSpawn =
          [](std::string clientId, int x, int y, int z) { return true; };
    } player;

    struct {
      /**
       * @brief This hook is called to load the map on client side.
       * The part of the map that must be loaded is that related to the space
       * under the authority of the grape whose id is passed as a parameter.
       */
      std::function<bool(std::string)> loadGrape = [](std::string) {
        return true;
      };

      /**
       * @brief This hook is called by __rp_loadExistingEntities to load the
       * entities that are already registered to the server.
       */
      std::function<bool(std::string, boost::json::array)>
          onLoadExistingEntities =
              [](std::string grapeId, boost::json::array summary) {
                return true;
              };
    } grape;

    /**
     * @brief this collection of hooks regroups all hooks related to managing
     * the passing of authority over entities.
     */
    struct {
      /**
       * @brief This hook is called when the client is informed that an entity
       * has been assigned to a new chunk.
       */
      std::function<void(std::string, std::string chunkId)> onTake =
          [](std::string entityId, std::string chunkId) {};
    } authority;

    /**
     * @brief This collection of hooks regroups all hooks related to
     * replication on the client side.
     */
    struct {
      /**
       * @brief This hook is called when the client receives replication data.
       *
       * @param entityId The id of the entity that the data is related to.
       * @param dataBlob The data that has been received.
       */
      std::function<void(std::string, std::string)> onReplicationDataReceived =
          [](std::string entityId, std::string dataBlob) {};

      /**
       * @brief This hook is called when the client receives active replication
       * data.
       *
       * @param entityId The id of the entity that the data is related to.
       * @param dataBlob The data that has been received.
       */
      std::function<void(std::string, std::string)>
          onActiveReplicationDataReceived =
              [](std::string entityId, std::string dataBlob) {};
    } replication;
  } client;
#endif
  /**
   * @brief This function will call the hook if it is not null.
   *
   * @tparam T The type of the hook.
   * @tparam Args The type of the arguments to pass to the hook
   */
  template <typename T, typename... Args> bool Call(T &&hook, Args &&...args) {
    if constexpr (std::is_member_function_pointer_v<std::decay_t<T>>) {
      if (hook) {
        return std::invoke(std::forward<T>(hook), std::forward<Args>(args)...);
      }
    } else {
      if (hook) {
        return hook(std::forward<Args>(args)...);
      }
    }
    return false;
  }
};

} // namespace api
} // namespace celte