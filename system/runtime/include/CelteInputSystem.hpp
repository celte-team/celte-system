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
            // STRUCTURES

            /**
             * @brief Structure of what is Stored in the circular buffer
             * @param status is the state of the input (pressed or not)
             * @param timestamp is the time when the input was pressed from the client side
             *
             * X and Y are optional and can be used for mouse position, joystick position, etc...
             * @param x is the x position of the input
             * @param y is the y position of the input
             */
            typedef struct DataInput_s {
                bool status;
                std::chrono::time_point<std::chrono::system_clock> timestamp;
                float x;
                float y;
            } DataInput_t;

            /**
             * @brief Structure of what is send by the SDK/Godot client
             * @param name is the name of the input (ex: "jump", "shoot", "move")
             * @param pressed is the state of the input (pressed or not)
             * @param uuid is the player id
             *
             * X and Y are optional and can be used for mouse position, joystick position, etc...
             * @param x is the x position of the input
             * @param y is the y position of the input
             * @param timestamp is the time when the input was pressed from the client side
             */
            typedef struct InputUpdate_s : public celte::net::CelteRequest<InputUpdate_s> {
                std::string name;
                bool pressed;
                std::string uuid; // player id
                float x;
                float y;
                int64_t timestamp;

                // Used to serialize the structure and send it through the network
                void to_json(nlohmann::json& j) const
                {
                    j = nlohmann::json { { "name", name }, { "pressed", pressed }, { "uuid", uuid }, { "x", x }, { "y", y } };
                }

                // Used to deserialize the structure and receive it from the network
                void from_json(const nlohmann::json& j)
                {
                    j.at("name").get_to(name);
                    j.at("pressed").get_to(pressed);
                    j.at("uuid").get_to(uuid);
                    j.at("x").get_to(x);
                    j.at("y").get_to(y);
                }
            } InputUpdate_t;

            /**
             * @brief List of InputData, used of custome struct instead of normal std::vector for custom serialization and deserialization
             * @param data is a list of InputUpdate_t
             */
            struct InputUpdateList_t : public celte::net::CelteRequest<InputUpdateList_t> {
                std::vector<InputUpdate_t> data;

                void to_json(nlohmann::json& j) const
                {
                    j = nlohmann::json { { "data", data } };
                }

                void from_json(const nlohmann::json& j)
                {
                    j.at("data").get_to(data);
                }
            };

            // TYPEDEF of the circular buffer for different use cases

            typedef std::map<std::string, std::map<std::string, boost::circular_buffer<DataInput_t>>> LIST_INPUTS;
            typedef std::map<std::string, boost::circular_buffer<DataInput_t>> LIST_INPUT_BY_UUID;
            typedef boost::circular_buffer<DataInput_t> INPUT;

            /**
             * @brief Construct a new Celte Input System object
             *
             * @param _io is the boost asio io_service, used to run WriteStreamPool.
             */
            CelteInputSystem(boost::asio::io_service& _io);

            /**
             * @brief Handle the input from the client.
             * Store them in a circular buffer.
             *
             * @param inputs is the list of inputs from the client
             */
            void HandleInput(InputUpdateList_t inputs);

            // GETTERS

            /**
             * @brief Get the List Input object
             *
             * @return std::shared_ptr<LIST_INPUTS> is the list of inputs
             */
            std::shared_ptr<LIST_INPUTS> GetListInput();

            /**
             * @brief Get the Writer Pool object
             *
             * @return net::WriterStreamPool&
             */
            net::WriterStreamPool& GetWriterPool();

            /**
             * @brief Get the List Input Of Uuid object
             *
             * @param uuid is the player id
             * @return std::optional<const LIST_INPUT_BY_UUID> is the list of inputs of the player
             */
            std::optional<const CelteInputSystem::LIST_INPUT_BY_UUID> GetListInputOfUuid(std::string uuid);

            /**
             * @brief Get the Input Circular Buf object
             *
             * @param uuid is the player id
             * @param InputName is the name of the input
             * @return std::optional<const INPUT> is the input of the player
             */
            std::optional<const CelteInputSystem::INPUT> GetInputCircularBuf(std::string uuid, std::string InputName);

            /**
             * @brief Get the Specific Input object
             *
             * @param uuid is the player id
             * @param InputName is the name of the input
             * @param indexHisto is the index of the input in the circular buffer
             * @return std::optional<const DataInput_t> is the input of the player
             */
            std::optional<const CelteInputSystem::DataInput_t> GetSpecificInput(std::string uuid, std::string InputName, int indexHisto);

        private:
            std::shared_ptr<LIST_INPUTS> _data;
            boost::asio::io_service _io;
            net::WriterStreamPool _Wpool;
        };

    };
}
