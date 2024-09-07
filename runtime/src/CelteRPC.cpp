#include "CelteRPC.hpp"
#include "KafkaFSM.hpp"
#include "CelteRuntime.hpp"

namespace celte {
    namespace rpc {
        Table::Table()
        {
            _producer = __createRPCProducer();
        }

        std::shared_ptr<kafka::clients::producer::KafkaProducer> Table::__createRPCProducer()
        {
            // Copying the defaults to add some custom settings
            auto props = nl::AKafkaLink::kDefaultProps;
            // props.put("delivery.timeout.ms", "100"); // TODO make this configurable

            // Writting to master's welcome channel to signal our arrival
            return std::make_shared<kafka::clients::producer::KafkaProducer>(props);
        }

        void Table::InvokeLocal(kafka::clients::consumer::ConsumerRecord record)
        {
            // Retrieve the RPC name from the header
            std::string rpcName("missing header rpName");
            for (auto& header : record.headers()) {
                if (header.key == "rpName") {
                    rpcName = header.value.toString();
                    break;
                }
            }

            if (rpcs.find(rpcName) == rpcs.end()) {
                std::cerr << "No RPC registered with name: " << rpcName << std::endl;
                return;
            }

            // Retrieve the RPC arguments from the record
            std::string serializedArguments = record.value().toString();

            // Invoke the RPC
            rpcs[rpcName].call(serializedArguments);

        }
    } // namespace rpc

} // namespace celte
