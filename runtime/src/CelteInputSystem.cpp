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
#include "kafka/KafkaConsumer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"

namespace celte {
    namespace runtime {
        CelteInputSystem::CelteInputSystem()
        {
        }

        void CelteInputSystem::RegisterInputCallback(const std::vector<std::string>& chunkId)
        {
            for (auto& topic : chunkId) {
                KPOOL.RegisterTopicCallback(
                    // Parsing the record to extract the new values of the properties, and
                    // updating the entity
                    topic,
                    [this, topic](const kafka::clients::consumer::ConsumerRecord& record) {
                        std::string resultSerialized(static_cast<const char*>(record.value().data()),
                            record.value().size());
                        std::string name;
                        bool pressed;
                        celte::rpc::unpack<std::string, bool>(resultSerialized, name, pressed);

                        handleInput(topic.substr(0, topic.find('.')), name, pressed);
                    });
            }
        }

        void CelteInputSystem::handleInput(std::string ChunkID, std::string InputName, bool status)
        {
            std::cout << "chunk: " << ChunkID << " | input name: " << InputName << " | pressed: " << status << std::endl;

            if (_data.find(ChunkID) == _data.end())
                _data[ChunkID] = std::map<std::string, boost::circular_buffer<DataInput_t>>();

            if (_data[ChunkID].find(InputName) == _data[ChunkID].end())
                _data[ChunkID][InputName] = boost::circular_buffer<DataInput_t>(10);

            DataInput_t newInput = { status, std::chrono::system_clock::now() };
            _data[ChunkID][InputName].push_back(newInput);
        }

        CelteInputSystem::LIST_INPUTS& CelteInputSystem::getListInput()
        {
            return _data;
        }
    }
}
