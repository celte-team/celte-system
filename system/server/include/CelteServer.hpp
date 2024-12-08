#pragma once
#include "ServerEvents.hpp"
#include "tinyfsm.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace celte {
namespace server {
/**
 * @brief Base class for all server's states.
 * Each state inheriting from this class should implement a particular
 * time of the server's lifecycle. See ServerStatesDeclaration.hpp for
 */
class AServer : public tinyfsm::Fsm<AServer> {
  friend class Fsm;

public:
  // ==========================================================================
  // Event reactions
  // ==========================================================================
  inline virtual void react(tinyfsm::Event const &) {
    std::cerr << "Unhandled client fsm event" << std::endl;
  };

  virtual void react(EConnectionSuccess const &event);
  virtual void react(EDisconnectFromServer const &event);
  virtual void react(EConnectToCluster const &event);

  // ==========================================================================
  // Entry points
  // ==========================================================================
  virtual void entry(void) = 0;
  virtual void exit(void) = 0;
};
} // namespace server
} // namespace celte