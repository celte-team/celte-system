#include "CelteInputSystem.hpp"

#include <iostream>
#include <string>

namespace celte {

CelteInputSystem &CelteInputSystem::GetInstance() {
  static CelteInputSystem instance;
  return instance;
}

CelteInputSystem::CelteInputSystem()
    : _Wpool(celte::net::WriterStreamPool::Options{
          .idleTimeout = std::chrono::milliseconds(10000)}) {
  _data = std::make_shared<LIST_INPUTS>();
}

void CelteInputSystem::HandleInput(std::string uuid, std::string InputName,
                                   bool status, float x, float y) {
  if (_data->find(uuid) == _data->end()) {
    (*_data)[uuid] =
        std::map<std::string, boost::circular_buffer<DataInput_t>>();
  }

  if ((*_data)[uuid].find(InputName) == (*_data)[uuid].end()) {
    (*_data)[uuid][InputName] = boost::circular_buffer<DataInput_t>(10);
  }

  DataInput_t newInput = {status, std::chrono::system_clock::now(), x, y};
  (*_data)[uuid][InputName].push_back(newInput);
}

std::shared_ptr<CelteInputSystem::LIST_INPUTS>
CelteInputSystem::GetListInput() {
  return _data;
}

std::optional<const CelteInputSystem::LIST_INPUT_BY_UUID>
CelteInputSystem::GetListInputOfUuid(std::string uuid) {
  auto uuidIt = _data->find(uuid);
  if (uuidIt != _data->end()) {
    return std::make_optional(uuidIt->second);
  }

  return std::nullopt;
}

std::optional<const CelteInputSystem::INPUT>
CelteInputSystem::GetInputCircularBuf(std::string uuid, std::string InputName) {
  auto inputMap = GetListInputOfUuid(uuid);
  if (inputMap) {
    auto inputIt = inputMap->find(InputName);
    if (inputIt != inputMap->end()) {
      return std::make_optional(inputIt->second);
    }
  }
  return std::nullopt;
}

std::optional<const CelteInputSystem::DataInput_t>
CelteInputSystem::GetSpecificInput(std::string uuid, std::string InputName,
                                   int indexHisto) {
  auto inputIt = GetInputCircularBuf(uuid, InputName);
  if (inputIt) {
    if (indexHisto >= 0 && indexHisto < inputIt->size()) {
      return std::make_optional(inputIt->at(indexHisto));
    }
  }
  return std::nullopt;
}

net::WriterStreamPool &CelteInputSystem::GetWriterPool() { return _Wpool; }

} // namespace celte
