/*
** CELTE, 2024
** celte-system

** Team Members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie

** File description:
** CelteInputSystem
*/

#pragma once

#include "CelteRequest.hpp"
#include "WriterStreamPool.hpp"
#include <boost/circular_buffer.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace celte {
    namespace runtime {

        class CelteInputSystem {
        public:
            typedef struct DataInput_s {
                bool status;
                std::chrono::time_point<std::chrono::system_clock> timestamp;
            } DataInput_t;

            typedef struct InputUpdate_s : public celte::net::CelteRequest<InputUpdate_s> {
                std::string name;
                bool pressed;
                std::string uuid; // player id

                void to_json(nlohmann::json& j) const
                {
                    j = nlohmann::json { { "name", name }, { "pressed", pressed }, { "uuid", uuid } };
                }

                void from_json(const nlohmann::json& j)
                {
                    j.at("name").get_to(name);
                    j.at("pressed").get_to(pressed);
                    j.at("uuid").get_to(uuid);
                }
            } InputUpdate_t;

            typedef std::map<std::string, std::map<std::string, boost::circular_buffer<DataInput_t>>> LIST_INPUTS;
            typedef std::map<std::string, boost::circular_buffer<DataInput_t>> LIST_INPUT_BY_UUID;
            typedef boost::circular_buffer<DataInput_t> INPUT;

            // CelteInputSystem();
            CelteInputSystem(boost::asio::io_service& _io);
            // void HandleInputCallback(const std::vector<std::string>& chunkId);
            void HandleInput(std::string ChunkID, std::string InputName, bool status);

            std::shared_ptr<LIST_INPUTS> GetListInput();
            net::WriterStreamPool& GetWriterPool();
            std::optional<const CelteInputSystem::LIST_INPUT_BY_UUID> GetListInputOfUuid(std::string uuid);
            std::optional<const CelteInputSystem::INPUT> GetInputCircularBuf(std::string uuid, std::string InputName);
            std::optional<const CelteInputSystem::DataInput_t> GetSpecificInput(std::string uuid, std::string InputName, int indexHisto);

        private:
            std::shared_ptr<LIST_INPUTS> _data;
            boost::asio::io_service _io;
            net::WriterStreamPool _Wpool;
        };

    };
}
