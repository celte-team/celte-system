#pragma once
#include <functional>
#include <kafka/KafkaConsumer.h>
#include <string>

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
  /**
   * @brief Singleton pattern for the HooksTable.
   */
  static HooksTable &HOOKS();

#ifdef CELTE_SERVER_MODE_ENABLED
  struct {
  } server;
#else
  struct {
    struct {

    } onServerConnected;
  } client;
#endif
};
} // namespace api
} // namespace celte