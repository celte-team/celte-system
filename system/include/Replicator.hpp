/*
** CELTE, 2025
** celte-system

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** Replicator
*/

#pragma once
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace celte {
namespace runtime {

class Replicator {
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

  /**
   * @brief Register a value to be replicated.
   *
   * @param name of the value to be replicated
   * @param get  a function that returns the value to be replicated
   * @param set  a function that sets the value to be replicated
   */
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
