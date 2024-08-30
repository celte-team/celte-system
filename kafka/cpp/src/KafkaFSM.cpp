#include "KafkaFSM.hpp"
#include "kafka/Properties.h"

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