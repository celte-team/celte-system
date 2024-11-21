#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"
#include "Logger.hpp"

namespace celte {
namespace rpc {
Table::Table() {}

void Table::InvokeLocal(kafka::clients::consumer::ConsumerRecord record) {
  try {
    // we try to execute the rpc
    std::string rpName = __getHdrValue__(record, "rpName");
    std::cout << "RPC: invoking " << rpName << std::endl;
    __tryInvokeRPC(record, rpName);

    // if no rpc with this name is found, it might be an answer to a previous
    // rpc (return value)
  } catch (std::runtime_error &e) {
    try {
      std::string rpcUUID = __getHdrValue__(record, "answer");
      __handleRPCReturnedValue(record, rpcUUID);
    } catch (std::runtime_error &e) {
      // if this fails to, then it is an error.
      logs::Logger::getInstance().err() << e.what() << std::endl;
    }
  }
}

void Table::__tryInvokeRPC(kafka::clients::consumer::ConsumerRecord record,
                           const std::string &rpName) {
  // Does the rpc exist?
  if (rpcs.find(rpName) == rpcs.end()) {
    logs::Logger::getInstance().err()
        << "No RPC registered with name: " << rpName << std::endl;
    return;
  }

  // Retrieve the RPC arguments from the record
  std::string serializedArguments(
      static_cast<const char *>(record.value().data()), record.value().size());

  // Invoke the RPC
  rpcs[rpName].call(record, serializedArguments);
}

void Table::__handleRPCReturnedValue(
    kafka::clients::consumer::ConsumerRecord record,
    const std::string &rpcUUID) {

  std::string resultSerialized(static_cast<const char *>(record.value().data()),
                               record.value().size());
  if (rpcPromises.find(rpcUUID) != rpcPromises.end()) {
    // This will update the value of the promise's future.
    rpcPromises[rpcUUID]->set_value(resultSerialized);
    rpcPromises.erase(rpcUUID);
  } else {
    logs::Logger::getInstance().err()
        << "Invalid response to non existing query: " << rpcUUID << std::endl;
  }
}

void Table::__send(
    kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDelivered) {
  KPOOL.Send(record, onDelivered);
}

std::string
__getHdrValue__(const kafka::clients::consumer::ConsumerRecord &record,
                const std::string &key) {
  for (auto &header : record.headers()) {
    if (header.key == key) {
      return std::string(static_cast<const char *>(header.value.data()),
                         header.value.size());
    }
  }
  throw std::runtime_error("Header not found: " + key);
}
} // namespace rpc
} // namespace celte
