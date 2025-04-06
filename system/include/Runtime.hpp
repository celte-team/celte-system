#pragma once
#include "AsyncTaskScheduler.hpp"
#include "Config.hpp"
#include "ETTRegistry.hpp"
#include "HookTable.hpp"
#include "Logger.hpp"
#include "TrashBin.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <string>
#include <tbb/concurrent_queue.h>

#define RUNTIME celte::Runtime::GetInstance()

namespace celte {
class PeerService; // forward declaration

class Runtime {
public:
  static Runtime &GetInstance();
  Runtime();

  /* ---------------------- FUNCTIONS EXPOSED TO THE API ----------------------
   */

  /// @brief Connects to the cluster using environment variables.
  /// CELTE_HOST for the address. Port is the default pulsar port (6650).
  /// @note This function is blocking.
  void ConnectToCluster();

  /// @brief Connects to the cluster using the provided address and port.
  /// @param address The address of the cluster.
  /// @param port The port of the cluster.
  /// @note This function is blocking.
  void ConnectToCluster(const std::string &address, int port);

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Forces the disconnection of a client from the server.
  void ForceDisconnectClient(const std::string &clientId,
                             const std::string &payload);
#else
  /// @brief Disconnects this client from the cluster
  void Disconnect();
#endif

  /// @brief Executes the runtime loop once. Call this once per frame in the
  /// engine.
  void Tick();

  inline std::string GenUUID() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid id = gen();
    return boost::uuids::to_string(id);
  }

  /* ---------------------- FUNCTIONS FOR INTERNAL USE ---------------------- */

  /// @brief Registers a task that will run in the same thread as Runtime::Tick.
  /// @param task
  inline void ScheduleSyncTask(std::function<void()> task) {
    _syncTasks.push(task);
  }

  /// @brief Registers a task that will run in a separate thread. For I/O tasks,
  /// use ScheduleAsyncIOTask instead.
  /// @param task
  inline void ScheduleAsyncTask(std::function<void()> task) {
    _asyncScheduler.Schedule(task);
  }

  /// @brief Registers a task that will run in a separate thread. Optimized for
  /// I/O tasks.
  inline void ScheduleAsyncIOTask(std::function<void()> task) {
    _asyncIOTaskScheduler.Schedule(task);
  }

  /// @brief Returns the AsyncTaskScheduler instance.
  inline AsyncTaskScheduler &GetAsyncTaskScheduler() { return _asyncScheduler; }

  /// @brief Returns the AsyncIOTaskScheduler instance.
  inline AsyncIOTaskScheduler &GetAsyncIOTaskScheduler() {
    return _asyncIOTaskScheduler;
  }

  /// @brief Returns the trash bin of the system.
  inline TrashBin &GetTrashBin() { return _trashBin; }

  /// @brief Returns the unique identifier of this peer on the network.
  inline const std::string &GetUUID() const { return _uuid; }

  /// @brief Returns the hook table.
  inline HookTable &Hooks() { return _hooks; }

  /// @brief Returns the peer service.
  /// @throws std::runtime_error if the peer service is not initialized.
  inline PeerService &GetPeerService() {
    if (!_peerService) {
      throw std::runtime_error("Peer service not initialized");
    }
    return *_peerService;
  }

  inline Config &GetConfig() { return _config; }

  inline Executor &TopExecutor() { return _topExecutor; }

#ifdef CELTE_SERVER_MODE_ENABLED
  /// @brief Returns the grape assigned to this server.
  inline const std::string &GetAssignedGrape() const { return _assignedGrape; }
  inline void SetAssignedGrape(const std::string &grape) {
    _assignedGrape = grape;
  }

  /// @brief Asks the master server to instantiate a new server node.
  /// @param payload The payload to forward to the newly instantiated node.
  void MasterInstantiateServerNode(const std::string &payload);
#endif

  /// @brief This binding lets the user register custom RPCs from the game
  /// engine. The arguments to the rpc will be passed a string (encoding of the
  /// args may vary). This rpc will be available on the global scope, and on the
  /// rpc topic of this peer. To register an rpc on the scope of a grape, use
  /// the grape registry.
  void RegisterCustomGlobalRPC(const std::string &name,
                               std::function<std::string(std::string)> f);

  void CallScopedRPCNoRetVal(const std::string &scope, const std::string &name,
                             const std::string &args);
  std::string CallScopedRPC(const std::string &scope, const std::string &name,
                            const std::string &args);
  void CallScopedRPCAsync(const std::string &scope, const std::string &name,
                          const std::string &args,
                          std::function<void(std::string)> callback);

private:
  /// @brief Advances the sync tasks queue.
  void __advanceSyncTasks();

  const std::string _uuid;
  tbb::concurrent_queue<std::function<void()>> _syncTasks;
  AsyncTaskScheduler _asyncScheduler;
  AsyncIOTaskScheduler _asyncIOTaskScheduler;
  HookTable _hooks;
  std::unique_ptr<PeerService> _peerService;
  Config _config;
  TrashBin _trashBin;

#ifdef CELTE_SERVER_MODE_ENABLED
  std::string _assignedGrape;
#endif

  Executor
      _topExecutor; ///< The executor for the top level tasks in the engine.
};
} // namespace celte
