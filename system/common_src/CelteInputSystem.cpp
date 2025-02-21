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

#include <iostream>
#include <string>

namespace celte {

    CelteInputSystem& CelteInputSystem::GetInstance()
    {
        static CelteInputSystem instance;
        return instance;
    }

    CelteInputSystem::CelteInputSystem()
        : _Wpool(
              celte::net::WriterStreamPool::Options {
                  .idleTimeout = std::chrono::milliseconds(10000) })
    {
        _data = std::make_shared<LIST_INPUTS>();
    }

    void CelteInputSystem::HandleInput(std::string uuid, std::string InputName,
        bool status, float x, float y)
    {
        // Dereference _data to access the map
        if (_data->find(uuid) == _data->end()) {
            // If the uuid is not found, create a new entry for it
            (*_data)[uuid] = std::map<std::string, boost::circular_buffer<DataInput_t>>();
        }

        if ((*_data)[uuid].find(InputName) == (*_data)[uuid].end()) {
            // If the InputName is not found, create a new circular buffer for it
            (*_data)[uuid][InputName] = boost::circular_buffer<DataInput_t>(10);
        }

        // Create a new DataInput_t with the given status and current time
        DataInput_t newInput = { status, std::chrono::system_clock::now(), x, y };

        // Push the new input to the circular buffer for the given uuid and InputName
        (*_data)[uuid][InputName].push_back(newInput);
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

} // namespace celte
