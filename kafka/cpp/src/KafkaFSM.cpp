#include "KafkaFSM.hpp"
#include "KafkaLinkStatesDeclaration.hpp"
#include "kafka/Properties.h"

namespace tinyfsm {
    template <> void Fsm<celte::nl::AKafkaLink>::set_initial_state()
    {
        Fsm<celte::nl::AKafkaLink>::current_state_ptr
            = &_state_instance<celte::nl::states::KLDisconnected>::value;
    }
} // namespace tinyfsm

using namespace celte::nl;

std::unordered_map<std::string, AKafkaLink::Consumer> AKafkaLink::_consumers
    = {};

kafka::Properties AKafkaLink::kDefaultProps = kafka::Properties(
    kafka::Properties::PropertiesMap({ { "enable.idempotence", { "true" } } }));

void AKafkaLink::RegisterConsumer(const std::string& topic,
    AKafkaLink::ScheduledKCTask callback,
    AKafkaLink::CustomPropsUpdater propsUpdater)
{
    auto props = AKafkaLink::kDefaultProps;
    if (propsUpdater)
        propsUpdater(props);

    Consumer consumer = {
        .consumer = kafka::clients::consumer::KafkaConsumer(props),
        .dataHandler = (callback != nullptr) ? callback : [](auto) {},
    };

    consumer.consumer.subscribe({ topic });
}