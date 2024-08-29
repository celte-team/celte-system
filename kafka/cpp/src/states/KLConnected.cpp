#include "KafkaLinkStatesDeclaration.hpp"

using namespace celte::nl::states;

void KLConnected::entry()
{
    // TODO create polling thread
    std::cout << "Successfully connected to network." << std::endl;
}

void KLConnected::exit()
{
    // TODO terminate polling thread
}
