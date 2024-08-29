/*
** CELTE, 2024
** celte
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** NLUtils.cpp
*/

#include "NLUtils.hpp"

namespace celte {
    namespace utils {
        std::string endpointToString(boost::asio::ip::udp::endpoint endpoint)
        {
            return endpoint.address().to_string() + ":"
                + std::to_string(endpoint.port());
        }
    }
}