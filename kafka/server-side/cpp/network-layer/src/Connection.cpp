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
** Connection.cpp
*/

#include "Connection.hpp"
#include <iostream>

namespace celte {
    namespace net {
        Connection::Connection(
            boost::asio::ip::udp::endpoint endpoint, size_t bufferSize)
            : _endpoint(endpoint)
            , _buffer(bufferSize)
            , _bufferMutex(std::make_shared<boost::mutex>())
            , _lastSeen(std::chrono::system_clock::now())
        {
        }

        std::optional<Packet> Connection::GetNextMessage()
        {
            std::vector<unsigned char> message;
            {
                boost::mutex::scoped_lock lock(*_bufferMutex);
                auto separatorPos = std::search(_buffer.begin(), _buffer.end(),
                    std::begin(_separator), std::end(_separator));
                if (separatorPos == _buffer.end()) {
                    return std::nullopt;
                }
                message
                    = std::vector<unsigned char>(_buffer.begin(), separatorPos);
                _buffer.erase(
                    _buffer.begin(), separatorPos + _separator.size());
            }
            try {
                return Packet::Parse(message);
            } catch (PacketException& e) {
                std::cout << "error parsing message: " << e.what() << std::endl;
                std::cout << "Packet size: " << message.size() << std::endl;
                return std::nullopt;
            }
        }

        void Connection::AppendBytes(std::vector<unsigned char> bytes)
        {
            boost::mutex::scoped_lock lock(*_bufferMutex);
            _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
            _lastSeen = std::chrono::system_clock::now();
        }
    } // namespace net
} // namespace celte