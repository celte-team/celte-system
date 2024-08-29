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
** Config.cpp
*/

#include "Config.hpp"

namespace celte {
    namespace config {
        void Config::Parse(int argc, char** argv)
        {
            for (int i = 1; i < argc; i += 2) {
                std::string argName = argv[i];
                if (argName.rfind("--", 0) == 0) {
                    argName = argName.substr(2); // Remove the '--' prefix
                } else {
                    throw std::invalid_argument(
                        "Arguments must start with '--'");
                }

                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        "Argument value missing for " + argName);
                }

                std::string argValue = argv[i + 1];
                // Check if the argument is a valid argument (either mandatory
                // or optional)
                __validateArgument(argName, argValue);
            }
            // Check if all mandatory arguments are present
            __validateMandatoryArguments();
        }

        void Config::__validateArgument(
            const std::string& name, const std::string& value)
        {
            if (mandatoryArgumentCheckers.find(name)
                != mandatoryArgumentCheckers.end()) {
                if (!mandatoryArgumentCheckers[name](name, value, true)) {
                    throw std::invalid_argument(
                        "Invalid value for mandatory argument: " + name);
                }
            } else if (optionalArgumentsCheckers.find(name)
                != optionalArgumentsCheckers.end()) {
                if (!optionalArgumentsCheckers[name](name, value, false)) {
                    throw std::invalid_argument(
                        "Invalid value for optional argument: " + name);
                }
            } else {
                throw std::invalid_argument("Unknown argument: " + name);
            }
        }

        void Config::__validateMandatoryArguments()
        {
            for (const auto& [name, checker] : mandatoryArgumentCheckers) {
                if (arguments.find(name) == arguments.end()) {
                    throw std::invalid_argument(
                        "Missing mandatory argument: " + name);
                }
            }
        }

        void Config::PrintHelp()
        {
            std::cout << "Usage: " << std::endl;
            for (const auto& [name, checker] : mandatoryArgumentCheckers) {
                std::cout << "--" << name << std::endl;
            }
            for (const auto& [name, checker] : optionalArgumentsCheckers) {
                std::cout << "--" << name << " (default: "
                          << std::any_cast<std::string>(arguments[name]) << ")"
                          << std::endl;
            }
        }
    } // namespace config
} // namespace celte