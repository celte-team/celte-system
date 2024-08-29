/*
** CELTE, 2023
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
** Config.hpp
*/

#pragma once
#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace celte {
    namespace config {
        /**
         * @brief This class is used to build the
         * configuration for a program. The developer can
         * specify mandatory arguments and optional arguments,
         * and the class will parse the command line arguments
         *
         */
        class Config {
        public:
            /**
             * @brief Adds a check for a mandatory argument.
             * The check is a lambda function that takes a string
             * and returns a boolean set to true if the string is
             * a valid value for the argument.
             *
             * @tparam T
             * @param name
             * @param description
             * @return Config&
             */
            template <typename T>
            Config& AddMandatoryArgument(
                std::string name, std::string description)
            {
                mandatoryArgumentCheckers[name] = __buildChecker<T>();
                return *this;
            }

            /**
             * @brief Adds a check for an optional argument.
             * The check is a lambda function that takes a string
             * and returns a boolean set to true if the string is
             * a valid value for the argument.
             *
             * @tparam T
             * @param name
             * @param description
             * @param defaultValue
             * @return Config&
             */
            template <typename T>
            Config& AddOptionalArgument(std::string name,
                std::string description, std::string defaultValue)
            {
                optionalArgumentsCheckers[name] = __buildChecker<T>();
                // Converting default value to T and assign it to the arguments
                // map
                std::istringstream ss(defaultValue);
                T val;
                ss >> val;
                arguments[name] = val;
                return *this;
            };

            /**
             * @brief This method assumes that the command line arguments
             * are in the form of "--name value". It will parse the arguments
             * and store them in the arguments map.
             *
             * @param argc
             * @param argv
             */
            void Parse(int argc, char** argv);

            template <typename T> T Get(std::string name)
            {
                std::cout << "Getting argument: " << name << std::endl;
                auto val = std::any_cast<T>(arguments[name]);
                std::cout << "Value: " << val << std::endl;
                return val;
            }

            void PrintHelp();

        private:
            /**
             * @brief Template method to build a checker for a
             * specific type. The method will return a lambda
             * function that takes a string and returns a boolean
             * set to true if the string is a valid value for the
             * argument's type.
             *
             * @tparam T
             * @return std::function<bool(std::string, std::string, bool)>
             */
            template <typename T>
            std::function<bool(std::string, std::string, bool)> __buildChecker()
            {
                return [this](std::string name, std::string value,
                           bool isOptional) {
                    std::istringstream ss(value);
                    T val;
                    ss >> val;
                    if (ss.fail()) {
                        return false;
                    }
                    arguments[name] = val;
                    return true;
                };
            }

            void __validateArgument(
                const std::string& name, const std::string& value);
            void __validateMandatoryArguments();

            std::map<std::string, std::any> arguments;
            std::map<std::string,
                std::function<bool(std::string, std::string, bool)>>
                mandatoryArgumentCheckers;
            std::map<std::string,
                std::function<bool(std::string, std::string, bool)>>
                optionalArgumentsCheckers;
        };

    } // namespace config
} // namespace celte