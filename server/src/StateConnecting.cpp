#include "ServerStatesDeclaration.hpp"

namespace celte {
    namespace server {
        namespace states {
            void Connecting::entry()
            {
                std::cerr << "Entering StateConnecting" << std::endl;
            }

            void Connecting::exit()
            {
                std::cerr << "Exiting StateConnecting" << std::endl;
            }

            void Connecting::react(EConnectionSuccess const& event)
            {
                // TODO initialize basic client consumers and producers here
                transit<Connected>();
            }
        } // namespace states
    } // namespace client
} // namespace celte