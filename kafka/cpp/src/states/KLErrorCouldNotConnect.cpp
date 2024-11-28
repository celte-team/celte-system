#include "KafkaLinkStatesDeclaration.hpp"

using namespace celte::nl::states;

void KLErrorCouldNotConnect::entry()
{
    // Nothing to do, waiting for the error to be handled
    std::cout << "Connection error. Network services entered error state. "
                 "Please restart."
              << std::endl;
}

void KLErrorCouldNotConnect::exit()
{
    // No clean up needed.
}
