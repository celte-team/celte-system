#pragma once
#include "ClientEvents.hpp"
#include "Logger.hpp"
#include "tinyfsm.hpp"
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace celte {
namespace client {
/**
 * @brief Base class for all client's states.
 * Each state inheriting from this class should implement a particular
 * time of the client's lifecycle. See ClientStatesDeclaration.hpp for
 * more information.
 */
class AClient : public tinyfsm::Fsm<AClient> {
  friend class Fsm;

public:
  // ==========================================================================
  // Event reactions
  // ==========================================================================
  inline virtual void react(tinyfsm::Event const &) {
    logs::Logger::getInstance().err()
        << "Unhandled client fsm event" << std::endl;
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
} // namespace client
} // namespace celte