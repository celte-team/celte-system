#pragma once

#include "WriterStreamPool.hpp"
#include "systems_structs.pb.h" // Include the generated protobuf header
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
  static CelteInputSystem &GetInstance();

  typedef struct DataInput_s {
    bool status;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    float x;
    float y;
  } DataInput_t;

  typedef struct InputUpdate_s {
    std::string name;
    bool pressed;
    std::string uuid; // player id
    float x;
    float y;

  } InputUpdate_t;

  typedef std::map<std::string,
                   std::map<std::string, boost::circular_buffer<DataInput_t>>>
      LIST_INPUTS;
  typedef std::map<std::string, boost::circular_buffer<DataInput_t>>
      LIST_INPUT_BY_UUID;
  typedef boost::circular_buffer<DataInput_t> INPUT;

  // CelteInputSystem();
  CelteInputSystem();
  // void HandleInputCallback(const std::vector<std::string>& chunkId);
  void HandleInput(std::string ChunkID, std::string InputName, bool status,
                   float x, float y);

  std::shared_ptr<LIST_INPUTS> GetListInput();
  net::WriterStreamPool &GetWriterPool();
  std::optional<const CelteInputSystem::LIST_INPUT_BY_UUID>
  GetListInputOfUuid(std::string uuid);
  std::optional<const CelteInputSystem::INPUT>
  GetInputCircularBuf(std::string uuid, std::string InputName);
  std::optional<const CelteInputSystem::DataInput_t>
  GetSpecificInput(std::string uuid, std::string InputName, int indexHisto);
  req::InputUpdate CreateInputUpdate(const std::string &name, bool pressed,
                                     const std::string &uuid, float x, float y);

private:
  std::shared_ptr<LIST_INPUTS> _data;
  boost::asio::io_service _io;
  net::WriterStreamPool _Wpool;
};

} // namespace celte
