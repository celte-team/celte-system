#pragma once
#include "unordered_map"
#include <msgpack.hpp>
#include <stdexcept>
#include <string>

namespace celte {
namespace runtime {

/**
 * @brief Replicator is a class that manages the replication of data between
 * peers. It is used to replicate data between peers in a distributed system.
 * It is does not handle the networking aspect of replication, only the
 * serialization and deserialization of the data, as well as keeping track
 * of changes.
 */
class Replicator {
private:
  /**
   * @brief ReplData contains information about the data that can be
   * replicated. It contains the size of the data, a pointer to the data and
   * a flag to indicate if the data has changed.
   */
  struct ReplData {
    size_t dataSize;
    void *dataPtr;
    bool hasChanged;
  };

  struct ActiveReplData {
    size_t dataSize;
    void *dataPtr;
    int hash;
  };

public:
  /**
   * @brief ReplBlob is a structure that contains the data that needs to be
   * replicated, as well as the keys that have changed in the data.
   * The contents of the new data serialized as {key: value, key: value,
   *
   */
  using ReplBlob = std::string;

  /**
   * @brief Acknowledge that the value has chnaged and that it should be
   * replicated to other peers.
   */
  void notifyDataChanged(const std::string &name);

  /**
   * @brief Resets the flag that indicates that the data has changed for all
   * data.
   */
  void ResetDataChanged();

  /**
   * @brief Returns the a serialized version of the data to be replicated.
   *
   */
  ReplBlob GetBlob();

  /**
   * @brief Returns a blob containing all of the changes to the data that is
   * being actively watched for.
   */
  ReplBlob GetActiveBlob();

  /**
   * @brief Overwrite the data with the data in the blob.
   */
  void Overwrite(const ReplBlob &blob, bool active = false);

  /**
   * @brief Registers a value to be replicated.
   *
   */
  template <typename T> void registerValue(const std::string &name, T &value) {
    if (_replicatedData.find(name) != _replicatedData.end()) {
      throw std::runtime_error("Value already registered: " + name);
    }
    ReplData replData = {sizeof(value), &value, false};
    _replicatedData[name] = replData;
  }

  /**
   * @brief Registers a value to be replicated. Contrary to registerValue, this
   * value will be actively watched over, meaning that any change to the value
   * will be sent over the network without needing to call notifyDataChanged.
   */
  template <typename T>
  void registerActiveValue(const std::string &name, T &value) {
    if (_activeReplicatedData.find(name) != _activeReplicatedData.end()) {
      throw std::runtime_error("Value already registered: " + name);
    }
    ActiveReplData replData = {sizeof(value), &value, false};
    _activeReplicatedData[name] = replData;
  }

  /**
   * @brief Get the value of a registered value.
   */
  template <typename T> T getValue(const std::string &name) {
    return *static_cast<T *>(_replicatedData[name].dataPtr);
  }

private:
  int __computeCheckSum(void *dataptr, size_t size);

  /**
   * @brief This method is pretty much the same than __overwriteActiveData,
   * except the destination is _replicatedData instead of _activeReplicatedData.
   * This is code duplication, so not cool, but it will avoid a LOT of
   * conditional statements at runtime.
   * This will take a blob and update the local values of the properties
   * described by the blob.
   */
  void __overwriteData(const ReplBlob &blob, msgpack::unpacker &unpacker);

  /**
   * @brief This method is pretty much the same than __overwriteData,
   * except the destination is _activeReplicatedData instead of _replicatedData.
   * This is code duplication, so not cool, but it will avoid a LOT of
   * conditional statements at runtime.
   * This will take a blob and update the local values of the properties
   * described by the blob.
   */
  void __overwriteActiveData(const ReplBlob &blob, msgpack::unpacker &unpacker);

  // Values to replicate, key is the name of the value to replicate
  std::unordered_map<std::string, ReplData> _replicatedData;
  std::unordered_map<std::string, ActiveReplData> _activeReplicatedData;
};
} // namespace runtime
} // namespace celte