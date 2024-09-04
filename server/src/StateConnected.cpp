#include "ServerStatesDeclaration.hpp"

namespace celte {
    namespace server {
        namespace states {
            void Connected::entry()
            {
                std::cerr << "Entering StateConnected" << std::endl;
            }

            void Connected::exit()
            {
                std::cerr << "Exiting StateConnected" << std::endl;
            }

            void Connected::react(EDisconnectFromServer const& event)
            {
                std::cerr << "Disconnecting from server" << std::endl;
                transit<Disconnected>();
            }
        } // namespace states
    } // namespace client
} // namespace celte