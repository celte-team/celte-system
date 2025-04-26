#pragma once

#include "ETTRegistry.hpp"
#include "WriterStreamPool.hpp"
#include "systems_structs.pb.h"

#include <boost/circular_buffer.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>

#define CINPUT celte::CelteInputSystem::GetInstance()

namespace celte {

    class CelteInputSystem {
    public:
        static CelteInputSystem& GetInstance();

        typedef struct InputUpdate_s {
            std::string name;
            bool pressed;
            std::string uuid; // player id
            float x;
            float y;

        } InputUpdate_t;

        // CelteInputSystem();
        CelteInputSystem();
        // void HandleInputCallback(const std::vector<std::string>& chunkId);
        void HandleInput(std::string ChunkID, std::string InputName, bool status,
            float x, float y);

        net::WriterStreamPool& GetWriterPool();
        req::InputUpdate CreateInputUpdate(const std::string& name, bool pressed, const std::string& uuid, float x, float y);

    private:
        boost::asio::io_service _io;
        net::WriterStreamPool _Wpool;
    };

}
