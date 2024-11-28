#include <kafka/KafkaConsumer.h>

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <string>

std::atomic_bool running = { true };

void stopRunning(int sig)
{
    if (sig != SIGINT)
        return;

    if (running) {
        running = false;
    } else {
        // Restore the signal handler, -- to avoid stuck with this handler
        signal(SIGINT, SIG_IGN); // NOLINT
    }
}

int main(int ac, char* av[])
{
    using namespace kafka;
    using namespace kafka::clients::consumer;

    // Use Ctrl-C to terminate the program
    signal(SIGINT, stopRunning); // NOLINT

    // E.g. KAFKA_BROKER_LIST:
    // "192.168.0.1:9092,192.168.0.2:9092,192.168.0.3:9092"
    const std::string brokers = "localhost:80"; // NOLINT
    // const Topic topic = "test.topic"; // NOLINT
    const Topic topic = (ac > 1) ? std::string(av[1]) : "test.topic";

    // Prepare the configuration
    Properties props({ { "bootstrap.servers", { brokers } } });

    // // To print out the error
    // props.put("error_cb", [](const kafka::Error& error) {
    //     // https://en.wikipedia.org/wiki/ANSI_escape_code
    //     std::cerr << "\033[1;31m" << "[" << kafka::utility::getCurrentTime()
    //               << "] ==> Met Error: " << "\033[0m";
    //     std::cerr << "\033[4;35m" << error.toString() << "\033[0m" <<
    //     std::endl;
    // });

    // // To enable the debug-level log
    // props.put("log_level", "7");
    // props.put("debug", "all");
    // props.put("log_cb",
    //     [](int /*level*/, const char* /*filename*/, int /*lineno*/,
    //         const char* msg) {
    //         std::cout << "[" << kafka::utility::getCurrentTime() << "]" <<
    //         msg
    //                   << std::endl;
    //     });

    // // To enable the statistics dumping
    // props.put("statistics.interval.ms", "1000");
    // props.put("stats_cb", [](const std::string& jsonString) {
    //     std::cout << "Statistics: " << jsonString << std::endl;
    // });

    // Create a consumer instance
    KafkaConsumer consumer(props);

    // Subscribe to topics
    consumer.subscribe({ topic });

    while (running) {
        // Poll messages from Kafka brokers
        auto records = consumer.poll(std::chrono::milliseconds(100));

        for (const auto& record : records) {
            if (!record.error()) {
                std::cout << "Got a new message..." << std::endl;
                std::cout << "    Topic    : " << record.topic() << std::endl;
                std::cout << "    Partition: " << record.partition()
                          << std::endl;
                std::cout << "    Offset   : " << record.offset() << std::endl;
                std::cout << "    Timestamp: " << record.timestamp().toString()
                          << std::endl;
                std::cout << "    Headers  : " << toString(record.headers())
                          << std::endl;
                std::cout << "    Key   [" << record.key().toString() << "]"
                          << std::endl;
                std::cout << "    Value [" << record.value().toString() << "]"
                          << std::endl;
            } else {
                std::cerr << record.toString() << std::endl;
            }
        }
    }

    // No explicit close is needed, RAII will take care of it
    consumer.close();
}