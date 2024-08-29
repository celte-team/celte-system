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
** Packet.cpp
*/

#include "Packet.hpp"
#include <cstring>
#include <iostream>
#include <zlib.h>

namespace celte {
    namespace net {
        Packet::Packet(int opcode)
            : _header({ opcode, 0, 0 })
            , _body()
        {
        }

        Packet::Packet(Header header, std::vector<unsigned char> body)
            : _header(header)
            , _body(body)
        {
            if (header.checksum
                != crc32(0, (const Bytef*)&body[0], body.size())) {
                throw PacketException("Checksum does not match body");
            }
        }

        void Packet::computeChecksum()
        {
            _header.checksum = 0;
            _header.checksum = crc32(0, (const Bytef*)&_body[0], _body.size());
            _header.size = _body.size();
        }

        std::vector<unsigned char> Packet::Serialize()
        {
            computeChecksum();
            std::vector<unsigned char> data(sizeof(Header) + _body.size() + 2);
            std::memcpy(&data[0], &_header, sizeof(Header));
            std::memcpy(&data[sizeof(Header)], &_body[0], _body.size());
            data[data.size() - 2] = '\r';
            data[data.size() - 1] = '\n';
            return data;
        }

        Packet Packet::Parse(std::vector<unsigned char>& data)
        {
            // Removing \r\n if present
            if (data.size() >= 2 && data[data.size() - 2] == '\r'
                && data[data.size() - 1] == '\n') {
                data.pop_back();
                data.pop_back();
            }

            // If data is smaller that the size of a Header, error
            if (data.size() < sizeof(Header)) {
                throw PacketException(
                    "Packet is too small to contain a header");
            }

            // Extracting the header
            Header header;
            std::memcpy(&header, &data[0], sizeof(Header));
            // Extracting the body
            std::vector<unsigned char> body(
                data.begin() + sizeof(Header), data.end());

            // Constructing the packet
            return Packet(header, body);
        }

    } // namespace net
} // namespace celte