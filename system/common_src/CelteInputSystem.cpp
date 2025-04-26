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
        : _Wpool(celte::net::WriterStreamPool::Options {
              .idleTimeout = std::chrono::milliseconds(10000) })
    {
    }

    void CelteInputSystem::HandleInput(std::string uuid, std::string InputName,
        bool status, float x, float y)
    {
        ETTREGISTRY.RunWithLock(uuid, [&](celte::Entity& e) {
            if (e.inputs->find(InputName) == e.inputs->end()) {
                (*e.inputs)[InputName] = boost::circular_buffer<DataInput_t>(10);
            }

            celte::DataInput_t newInput = { status, std::chrono::system_clock::now(), x, y };
            (*e.inputs)[InputName].push_back({ status, std::chrono::system_clock::now(), x, y });
        });
    }

    net::WriterStreamPool& CelteInputSystem::GetWriterPool() { return _Wpool; }

} // namespace celte
