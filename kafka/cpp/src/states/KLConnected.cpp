#include "KafkaFSM.hpp"
#include "KafkaLinkStatesDeclaration.hpp"

using namespace celte::nl::states;

KLConnected::~KLConnected() { __stopPolling(); }

void KLConnected::entry() { __startPolling(); }

void KLConnected::exit() { __stopPolling(); }

void KLConnected::__stopPolling()
{
    _shouldPoll = false;
    if (_pollThread.joinable()) {
        _pollThread.join();
    }
}

void KLConnected::__startPolling()
{
    _shouldPoll = true;
    _pollThread = std::thread([this]() {
        while (_shouldPoll) {
            celte::nl::AKafkaLink::__pollAllConsumers();
            std::this_thread::sleep_for(
                celte::nl::AKafkaLink::kCelteConfig.pollingIntervalMs);
        }
    });
}