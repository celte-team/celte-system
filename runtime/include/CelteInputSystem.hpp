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

#include <boost/circular_buffer.hpp>
#include <chrono>
#include <map>
#include <string>

namespace celte {
    namespace runtime {

        class CelteInputSystem {
        public:
            typedef struct DataInput_s {
                bool status;
                std::chrono::time_point<std::chrono::system_clock> timestamp;
            } DataInput_t;

            typedef struct InputUpdate_s {
                std::string name;
                bool pressed;
            } InputUpdate_t;

            typedef std::map<std::string, std::map<std::string, boost::circular_buffer<DataInput_t>>> LIST_INPUTS;

            CelteInputSystem();
            void RegisterInputCallback(const std::vector<std::string>& chunkId);
            void handleInput(std::string ChunkID, std::string InputName, bool status);
            LIST_INPUTS& getListInput();

        private:
            LIST_INPUTS _data;
        };

    };
}
