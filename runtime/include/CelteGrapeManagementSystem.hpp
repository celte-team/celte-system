#pragma once
#include "CelteGrape.hpp"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

#define GRAPES celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()

namespace celte {
namespace chunks {
class CelteGrapeManagementSystem {
public:
  CelteGrapeManagementSystem() {}
  ~CelteGrapeManagementSystem() {
    std::cout << "Grape management system destructor called" << std::endl;
  }

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief Sets the interval in milliseconds at which Celte should send updates
   * for the state of the data that is being replicated.
   */
  inline void SetReplicationIntervalMs(int intervalMs) {
    _replicationIntervalMs = intervalMs;
  }
#endif

  /**
   * @brief Register a grape with the grape management
   * system. This will allow the grape management
   * system to manage the grape and its chunks.
   *
   * @param options: The options to configure the grape.
   * @return The grapeId of the registered grape.
   */
  Grape &RegisterGrape(const GrapeOptions &options);

  /**
   * @brief The global grape management system.
   * Access the singleton instance of the grape management
   * system using this function.
   */
  static CelteGrapeManagementSystem &GRAPE_MANAGER();

  /**
   * @brief Get a grape by its grapeId.
   *
   * @param grapeId: The grapeId of the grape to get.
   * @return The grape with the given grapeId.
   *
   * # EXCEPTIONS
   * If the grapeId does not exist, this function will throw
   * a std::out_of_range exception.
   */
  Grape &GetGrape(std::string grapeId);

  /**
   * @brief Get the grape of an object by its position.
   */
  Grape &GetGrapeByPosition(float x, float y, float z);

  Chunk &GetChunkById(const std::string &chunkId);

#ifdef CELTE_SERVER_MODE_ENABLED
  /**
   * @brief This method is called by the CelteEntityManagementSystem and will
   * send the replication data of all entities.
   */
  void ReplicateAllEntities();

  /**
   * @brief Returns true if the chunk must send replication data (the timer
   * has expired).
   */
  inline bool MustSendReplicationData() {
    return std::chrono::high_resolution_clock::now() -
               _lastReplicationDataSent >
           std::chrono::milliseconds(_replicationIntervalMs);
  }

  /**
   * @brief Resets the timer for the replication data.
   */
  inline void ResetReplicationDataTimer() {
    _lastReplicationDataSent = std::chrono::high_resolution_clock::now();
  }
#endif

private:
  std::unordered_map<std::string, std::shared_ptr<Grape>> _grapes;
#ifdef CELTE_SERVER_MODE_ENABLED
  std::chrono::time_point<std::chrono::high_resolution_clock>
      _lastReplicationDataSent;
  int _replicationIntervalMs = 10;
#endif
};
} // namespace chunks
} // namespace celte