/*
** CELTE, 2024
** cpp
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** Packet.hpp
*/

#pragma once
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace celte {
    namespace net {
        class PacketException : public std::runtime_error {
        public:
            PacketException(const std::string& message)
                : std::runtime_error(message)
            {
            }
        };

        /**
         * @brief Header for all messages sent over the network.
         * The opcode is the code of the remote procedure call that must
         * handle this message.
         * The size is the size of the message in bytes.
         * The checksum is the checksum of the message, computed using the
         * CRC32 algorithm.
         *
         * @example
         * @code {.c++}
         * Packet packet(1); // opcode 1
         * packet << 42; // add an integer to the packet
         * packet << 3.14; // add a float to the packet
         *
         * try {
         *      std::vector<unsigned char> data = packet.Serialize(); //
         * serialize the packet to bytes
         *
         *     Packet parsed = Packet::Parse(data); // parse the bytes to a
         * packet int i; float f; parsed >> i; // extract the integer from the
         * packet parsed >> f; // extract the float from the packet } catch
         * (PacketException& e) { std::cerr << e.what() << std::endl;
         * }
         *
         *
         * @endcode
         *
         */
        struct Header {
            int opcode;
            int size;
            unsigned long checksum;
        } __attribute__((packed));

        /**
         * @brief Packet class.
         * A packet is a message sent over the network.
         * It contains a header and a body.
         */
        struct Packet {
            Header _header;
            std::vector<unsigned char> _body;

            /**
             * @brief Construct a new Packet object
             *
             */
            Packet(int opcode);

            /**
             * @brief Construct a new Packet object
             *
             */
            Packet(Header header, std::vector<unsigned char> body);

            /**
             * @brief The << operator serializes data into the packet.
             *
             * @tparam T
             * @param data
             * @return Packet&
             */
            template <typename T> Packet& operator<<(T data)
            {
                static_assert(std::is_standard_layout<T>::value,
                    "Data is too complex to be serialized");
                size_t size = sizeof(T);
                size_t oldSize = _body.size();

                _body.resize(oldSize + size);
                std::memcpy(_body.data() + oldSize, &data, size);
                _header.size += size;
                return *this;
            }

            /**
             * @brief The >> operator deserializes data from the packet.
             *
             * @tparam T
             * @param data
             * @return Packet&
             */
            template <typename T> Packet& operator>>(T& data)
            {
                static_assert(std::is_standard_layout<T>::value,
                    "Data is too complex to be deserialized");
                size_t size = sizeof(T);
                size_t oldSize = _body.size();

                if (oldSize < size)
                    throw std::runtime_error("Not enough data to deserialize");

                std::memcpy(&data, _body.data(), size);
                _body.erase(_body.begin(), _body.begin() + size);
                _header.size -= size;
                return *this;
            }

            /**
             * @brief Computes the checksum of the packet.
             *
             */
            void computeChecksum();

            /**
             * @brief Serializes the packet to a vector of bytes.
             * The vector contains the header followed by the body.
             *
             */
            std::vector<unsigned char> Serialize();

            /**
             * @brief Constructs a packet from a vector of bytes.
             * If the checksum is invalid, or the size is incorrect, an
             * exception is thrown.
             *
             * # Exceptions
             * - PacketException: if the checksum is invalid or the size is
             * incorrect.
             *
             */
            static Packet Parse(std::vector<unsigned char>& data);
        };

    } // namespace net
} // namespace celte