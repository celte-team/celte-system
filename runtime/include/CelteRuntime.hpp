/*
 ** CELTE, 2024
 ** server-side
 **
 ** Team members:
 ** Eliot Janvier
 ** Clement Toni
 ** Ewen Briand
 ** Laurent Jiang
 ** Thomas Laprie
 **
 ** File description:
 ** CelteRuntime.hpp
 */

#ifndef CELTE_RUNTIME_HPP
#define CELTE_RUNTIME_HPP
#ifdef CELTE_SERVER_MODE_ENABLED
#include "CelteServer.hpp"
#include "ServerStatesDeclaration.hpp"
#else
#include "CelteClient.hpp"
#include "ClientStatesDeclaration.hpp"
#endif
#include "CelteGrape.hpp"
#include "CelteHooks.hpp"
#include "CelteRPC.hpp"
#include "KafkaPool.hpp"
#include "tinyfsm.hpp"
#include "topics.hpp"
#include <atomic>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

namespace celte {
namespace runtime {
enum RuntimeMode { SERVER, CLIENT };

#ifdef CELTE_SERVER_MODE_ENABLED
using Services =
    // tinyfsm::FsmList<celte::server::AServer, celte::nl::AKafkaLink>;
    tinyfsm::FsmList<celte::server::AServer>;
#else
using Services =
    // tinyfsm::FsmList<celte::client::AClient, celte::nl::AKafkaLink>;
    tinyfsm::FsmList<celte::client::AClient>;
#endif

#define RUNTIME celte::runtime::CelteRuntime::GetInstance()
#define HOOKS celte::runtime::CelteRuntime::GetInstance().Hooks()
#define RPC celte::runtime::CelteRuntime::GetInstance().RPCTable()
#define KPOOL celte::runtime::CelteRuntime::GetInstance().KPool()

/**
 * @brief This class contains all the logic necessary
 * for Celte to run in a Godot project.
 * Depending on the mode (client or server), the runtime
 * will get initialized differently to reduce checks at runtime.
 *
 */
class CelteRuntime {
public:
  /**
   * @brief Singleton pattern for the CelteRuntime.
   *
   */
  static CelteRuntime &GetInstance();

  CelteRuntime();
  ~CelteRuntime();

  /**
   * @brief Updates the state of the runtime synchronously.
   *
   */
  void Tick();

  /**
   * @brief Register a new callback to be executed every time the
   * Tick method is called.
   *
   * Returns a unique id for this callback.
   *
   * @param task
   *
   * @return int the id of the registered callback. Used to unregister it
   * with UnregisterTickCallback.
   */
  int RegisterTickCallback(std::function<void()> callback);

  /**
   * @brief Unregister a callback by its id.
   *
   * @param id
   */
  void UnregisterTickCallback(int id);

  /**
   * @brief Starts all Celte services
   *
   */
  void Start(RuntimeMode mode);

#ifndef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief This method is only available in client mode.
   * It requests the server to spawn a new entity for the player to possess.
   * The client should have an RPC defined called onAuthorizeSpawn that will
   * call a user defined hook responsible for spawning the entity.
   */
  void RequestSpawn(const std::string &clientId, const std::string &grapeId,
                    float x, float y, float z);
#endif

private:
// =================================================================================================
// SERVER INIT
// =================================================================================================
#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Initialize the Celte runtime in server mode.
   *
   */
  void __initServer();

  /**
   * @brief Initialize the RPC table specific to the server.
   */
  void __initServerRPC();
#endif

// =================================================================================================
// CLIENT INIT
// =================================================================================================
#ifndef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Initialize the Celte runtime in client mode.
   *
   */
  void __initClient();

  /**
   * @brief Initialize the RPC table specific to the client.
   */
  void __initClientRPC();
#endif

  // =================================================================================================
  // Services API
  // =================================================================================================
public:
  /**
   * @brief Attemps a connection to a kafka cluster.
   */
  void ConnectToCluster(const std::string &ip, int port);

  /**
   * @brief Returns true if the runtime is currently connected to
   * kafka
   */
  bool IsConnectedToCluster();

  /**
   * @brief Waits until the runtime is connected to the cluster. If timeout is
   * reached, returns false.
   *
   */
  bool WaitForClusterConnection(int timeoutMs);

  /**
   * @brief Returns the RPC table.
   *
   * @return rpc::Table&
   */
  rpc::Table &RPCTable();

  /**
   * @brief Returns a reference to the KafkaPool used to send and receive
   * messages from kafka. If the kafka pool is not initialized, throws an
   * std::logic_error.
   */
  nl::KafkaPool &KPool();

  /**
   * @brief Returns a reference to the hook table.
   */
  inline api::HooksTable &Hooks() { return _hooks; }

  /**
   * @brief Returns the UUID of the peer.
   */
  const std::string &GetUUID() const {
#ifdef CELTE_SERVER_MODE_ENABLED
    static const std::string PEER_UUID =
        "sn." + boost::uuids::to_string(
                    boost::uuids::random_generator()()); // random uuid for the
                                                         // peer to identify
                                                         // itself to the master
#else
    static const std::string PEER_UUID =
        "client." +
        boost::uuids::to_string(
            boost::uuids::random_generator()()); // random uuid for the peer to
                                                 // identify itself to the
                                                 // master
#endif
    return PEER_UUID;
  }

private:
  // =================================================================================================
  // PRIVATE METHODS
  // =================================================================================================

  /**
   * @brief Create the network layer, either in server or client
   * mode.
   *
   * @param mode
   */
  void __initNetworkLayer(RuntimeMode mode);

  // =================================================================================================
  // PRIVATE MEMBERS
  // =================================================================================================

  // list of tasks to be ran each frame.
  // The key is the id of the task, used to unregister it if needed.
  std::map<int, std::function<void()>> _tickCallbacks;

  // id of the next callback to be registered
  int _tickCallbackId = 0;

  // either server or client, should be decided at compilation
  RuntimeMode _mode;

  // The RPC table
  rpc::Table _rpcTable;

  // Kafka producer / consumer pool used to send and receive messages from kafka
  std::shared_ptr<nl::KafkaPool> _pool;

  // Hooks table
  api::HooksTable _hooks;
};
} // namespace runtime
} // namespace celte

#endif // CELTE_RUNTIME_HPP