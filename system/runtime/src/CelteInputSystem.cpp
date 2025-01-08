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

#include "CelteInputSystem.hpp"
#include "CelteRuntime.hpp"

#include <iostream>
#include <string>

namespace celte {
    namespace runtime {
        CelteInputSystem::CelteInputSystem(boost::asio::io_service& io)
            : _Wpool(
                  celte::net::WriterStreamPool::Options {
                      .idleTimeout = std::chrono::milliseconds(10000) },
                  io)
        {
            _data = std::make_shared<LIST_INPUTS>();
        }

        void CelteInputSystem::HandleInput(InputUpdateList_t inputs)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto& input : inputs.data) {
                if (_data->find(input.uuid) == _data->end())
                    // If the uuid is not found, create a new entry for it
                    (*_data)[input.uuid] = std::map<std::string, boost::circular_buffer<DataInput_t>>();

                if ((*_data)[input.uuid].find(input.name) == (*_data)[input.uuid].end())
                    (*_data)[input.uuid][input.name] = boost::circular_buffer<DataInput_t>(10);

                std::chrono::time_point<std::chrono::system_clock> time = std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(input.timestamp));
                DataInput_t newInput = { input.pressed, time, input.x, input.y };

                (*_data)[input.uuid][input.name].push_back(newInput);
            }
            // sort all the input uuid and input.name by timestamp
            // for (auto& uuid : *_data) {
            //     for (auto& input : uuid.second) {
            //         std::cout << "UUID: " << uuid.first << ", Input name: " << input.first << std::endl;
            //         for (const auto& data : input.second) {
            //             auto now = std::chrono::system_clock::now();
            //             auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.timestamp).count();
            //             std::cout << "Pressed: " << data.status
            //                       << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(data.timestamp.time_since_epoch()).count()
            //                       << ", Elapsed time: " << elapsed << " ms"
            //                       << ", X: " << data.x
            //                       << ", Y: " << data.y
            //                       << std::endl;
            //         }
            //     }
            // }
        }

        std::shared_ptr<CelteInputSystem::LIST_INPUTS>
        CelteInputSystem::GetListInput()
        {
            return _data;
        }

        std::optional<const CelteInputSystem::LIST_INPUT_BY_UUID>
        CelteInputSystem::GetListInputOfUuid(std::string uuid)
        {
            auto uuidIt = _data->find(uuid); // Find the UUID in the outer map
            if (uuidIt != _data->end()) {
                // Return the reference wrapped in std::optional if found
                return std::make_optional(uuidIt->second);
            }

            // Return std::nullopt if the UUID is not found
            return std::nullopt;
        }

        std::optional<const CelteInputSystem::INPUT>
        CelteInputSystem::GetInputCircularBuf(std::string uuid, std::string InputName)
        {
            auto inputMap = GetListInputOfUuid(uuid);
            if (inputMap) {
                auto inputIt = inputMap->find(InputName); // Find the InputName in the inner map
                if (inputIt != inputMap->end()) {
                    // Return a copy of the circular buffer wrapped in std::optional
                    return std::make_optional(inputIt->second);
                }
            }
            // If not found, return std::nullopt
            return std::nullopt;
        }

        std::optional<const CelteInputSystem::DataInput_t>
        CelteInputSystem::GetSpecificInput(std::string uuid, std::string InputName,
            int indexHisto)
        {
            auto inputIt = GetInputCircularBuf(uuid, InputName);
            if (inputIt) {
                // Ensure the index is valid for the circular buffer
                if (indexHisto >= 0 && indexHisto < inputIt->size()) {
                    // Return the specific DataInput_t wrapped in std::optional
                    return std::make_optional(inputIt->at(indexHisto));
                }
            }
            // If not found or index is out of bounds, return std::nullopt
            return std::nullopt;
        }

        net::WriterStreamPool& CelteInputSystem::GetWriterPool() { return _Wpool; }

    } // namespace runtime
} // namespace celte
