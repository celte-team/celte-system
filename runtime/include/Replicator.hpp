#pragma once
#include "unordered_map"
#include <iostream>
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
public:
  /**
   * @brief ReplBlob is a structure that contains the data that needs to be
   * replicated, as well as the keys that have changed in the data.
   * The contents of the new data serialized as {key: value, key: value,
   *
   */
  using ReplBlob = std::string;

  /**
   * @brief Returns a blob containing all of the changes to the data that is
   * being actively watched for.
   */
  ReplBlob GetBlob(bool peek = false);

  /**
   * @brief Overwrite the data with the data in the blob.
   */
  void Overwrite(const ReplBlob &blob);

  void RegisterReplicatedValue(const std::string &name,
                               std::function<std::string()> get,
                               std::function<void(const std::string &)> set);

private:
  int __computeCheckSum(const std::string &data);

  struct GetSet {
    std::function<std::string()> get;
    std::function<void(const std::string &)> set;
    int hash = 0;
  };

  std::unordered_map<std::string, GetSet> _replicatedValues;
};
} // namespace runtime
} // namespace celte