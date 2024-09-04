#include "ServerStatesDeclaration.hpp"

namespace celte {
    namespace server {
        namespace states {
            void Disconnected::entry()
            {
                std::cerr << "Client is Disconnected" << std::endl;
            }

            void Disconnected::exit() { }

            void Disconnected::react(celte::nl::EConnectToCluster const& event)
            {
                transit<Connecting>();
            }
        } // namespace states
    } // namespace client
} // namespace celte