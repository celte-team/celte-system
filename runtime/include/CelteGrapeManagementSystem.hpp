#pragma once
#include "CelteGrape.hpp"
#include <memory>
#include <string>
#include <unordered_map>

#define GRAPES celte::chunks::CelteGrapeManagementSystem::GRAPE_MANAGER()

namespace celte {
namespace chunks {
class CelteGrapeManagementSystem {
public:
  CelteGrapeManagementSystem() {}
  ~CelteGrapeManagementSystem() {}

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

private:
  std::unordered_map<std::string, std::shared_ptr<Grape>> _grapes;
};
} // namespace chunks
} // namespace celte