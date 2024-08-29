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
** NLUtils.hpp
*/

#pragma once
#include <boost/asio.hpp>
#include <string>

namespace celte {
    namespace utils {
        std::string endpointToString(boost::asio::ip::udp::endpoint endpoint);
    } // namespace utils
} // namespace celte