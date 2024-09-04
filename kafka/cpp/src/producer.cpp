#include "kafka/KafkaProducer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"
#include <string>

int main()
{
    using namespace kafka;
    using namespace kafka::clients::producer;
    const std::string brokers = "localhost:80";
    const Properties props({ { "bootstrap.servers", { brokers } },
        { "enable.idempotence", { "true" } } });
    const Topic topic = "test.topic";
    KafkaProducer producer(props);

    while (true) {
        // Prepare a message
        std::cout << "Type message value and hit enter to produce message..."
                  << std::endl;
        auto line = std::make_shared<std::string>();
        std::getline(std::cin, *line);

        // If ctrl-d is pressed, break the loop
        if (std::cin.eof()) {
            break;
        }

        ProducerRecord record(
            topic, NullKey, Value(line->c_str(), line->size()));

        // Prepare delivery callback
        auto deliveryCb
            // We capture line by value to keep it alive until the record's
            // value has been delivered
            = [line](const RecordMetadata& metadata, const Error& error) {
                  if (!error) {
                      std::cout << "Message delivered: " << metadata.toString()
                                << std::endl;
                  } else {
                      std::cerr << "Message failed to be delivered: "
                                << error.message() << std::endl;
                  }
              };

        // Send a message
        producer.send(record, deliveryCb);
    }

    // Close the producer explicitly(or not, since RAII will take care of it)
    producer.close();
}