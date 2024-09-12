#include "CelteRPC.hpp"
#include "CelteRuntime.hpp"

namespace celte {
namespace rpc {
Table::Table() {}

void Table::InvokeLocal(kafka::clients::consumer::ConsumerRecord record) {
  // Retrieve the RPC name from the header
  std::string rpcName("missing header rpName");
  for (auto &header : record.headers()) {
    if (header.key == "rpName") {
      rpcName = header.value.toString();
      break;
    }
  }

  if (rpcs.find(rpcName) == rpcs.end()) {
    std::cerr << "Error in InvokeLocal : ";
    std::cerr << "No RPC registered with name: " << rpcName << std::endl;

    std::cerr << "Available RPCs: " << std::endl;
    for (auto &rpc : rpcs) {
      std::cerr << "\t-" << rpc.first << std::endl;
    }
    return;
  }

  // Retrieve the RPC arguments from the record
  std::string serializedArguments(
      static_cast<const char *>(record.value().data()), record.value().size());

  // Invoke the RPC
  rpcs[rpcName].call(serializedArguments);
}

void Table::__send(
    const kafka::clients::producer::ProducerRecord &record,
    const std::function<void(const kafka::clients::producer::RecordMetadata &,
                             kafka::Error)> &onDeliveryError) {
  runtime::CelteRuntime::GetInstance().KPool().Send(record, onDeliveryError);
}

} // namespace rpc
} // namespace celte
