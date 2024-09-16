#pragma once
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
      std::function<bool(std::string, int, int, int)> spawnPlayer =
          [](std::string clientId, int x, int y, int z) { return true; };
    } newPlayerConnected;
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
    } connection;

    struct {
      /**
       * @brief This hook is called when the client is authorized to spawn a new
       * entity. It should implement the logic to spawn the entity in the game.
       */
      std::function<bool(std::string, int x, int y, int z)> onAuthorizeSpawn =
          [](std::string clientId, int x, int y, int z) { return true; };
    } player;
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