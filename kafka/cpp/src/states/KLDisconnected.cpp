#include "KafkaLinkStatesDeclaration.hpp"
#include "kafka/KafkaProducer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"

using namespace celte::nl::states;

void KLDisconnected::entry()
{
    // Nothing to do, waiting for EConnectToCluster to be dispatched
}

void KLDisconnected::exit()
{
    // No clean up needed.
}

void KLDisconnected::react(EConnectToCluster const& event)
{
    AKafkaLink::kDefaultProps.put("bootstrap.servers",
        event.ip + std::string(":") + std::to_string(event.port));

    // Copying the defaults to add some custom settings
    auto props = AKafkaLink::kDefaultProps;
    // props.put("delivery.timeout.ms", "10000");

    // Writting to master's welcome channel to signal our arrival
    kafka::clients::producer::KafkaProducer producer(props);

    // This object is used to send the message
    const kafka::Topic topic = "mas.welcome";
    kafka::clients::producer::ProducerRecord record(topic, kafka::NullKey,
        kafka::Value(event.message->c_str(), event.message->size()));

    auto message = std::move(event.message);

    // This callback gets called when the message has been successfully
    // delivered to kafka.
    auto deliveryCb
        = [this, message](
              const kafka::clients::producer::RecordMetadata& metadata,
              const kafka::Error& error) {
              if (!error) {
                  transit<KLConnected>();
              } else {
                  std::cout << "an error occured: " << error.message()
                            << std::endl;
                  transit<KLErrorCouldNotConnect>();
              }
          };

    producer.send(record, deliveryCb);
}