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
** Connection.hpp
*/

#pragma once
#include "Packet.hpp"
#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace celte {
    namespace net {
        /**
         * @brief Encapsulate a known connection, providing a
         * dedicated circular buffer to receive data from this connection.
         *
         */
        class Connection {
        public:
            Connection(
                boost::asio::ip::udp::endpoint endpoint, size_t bufferSize);

            /**
             * @brief Tries to find a separator \r\n in the buffer. If it finds
             * one, it returns the contents of the buffer up to the separator
             * and removes it from the buffer. The read cursor of the buffer is
             * updated to the next character after the separator. The result is
             * parsed into a celte::net::Packet object. If the buffer does not
             * contain a separator or if the message cannot be parsed into a
             * Packet, the function returns an empty optional.
             *
             * @return std::optional<Packet>
             */
            std::optional<Packet> GetNextMessage();

            /**
             * @brief Appends bytes to the buffer.
             *
             */
            void AppendBytes(std::vector<unsigned char> bytes);

            /**
             * @brief Returns the endpoint of the connection.
             *
             * @return boost::asio::ip::udp::endpoint
             */
            inline boost::asio::ip::udp::endpoint GetEndpoint() const
            {
                return _endpoint;
            }

            inline std::chrono::time_point<std::chrono::system_clock>
            GetLastSeen() const
            {
                return _lastSeen;
            }

        private:
            // The endpoint of the connection
            boost::asio::ip::udp::endpoint _endpoint;

            // Buffer holding the unprocessed data received from the connection
            boost::circular_buffer<unsigned char> _buffer;

            // Mutex to protect the buffer
            std::shared_ptr<boost::mutex> _bufferMutex;

            // Separator used to split messages in the buffer
            std::vector<unsigned char> _separator = { '\r', '\n' };

            // Time point of the last activity on the connection
            std::chrono::time_point<std::chrono::system_clock> _lastSeen;
        };
    } // namespace net
} // namespace celte