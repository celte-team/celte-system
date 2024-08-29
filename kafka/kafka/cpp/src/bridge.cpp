#include "kafka/KafkaProducer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

using boost::asio::ip::udp;
namespace po = boost::program_options;

void run_bridge(const std::string& listen_port, const std::string& produce_ip,
    const std::string& produce_port, const std::string& topic)
{
    using namespace kafka;
    using namespace kafka::clients::producer;

    // Kafka producer properties
    const Properties props(
        { { "bootstrap.servers", { produce_ip + ":" + produce_port } },
            { "enable.idempotence", { "true" } } });
    KafkaProducer producer(props);

    // Set up Boost.Asio
    boost::asio::io_context io_context;
    udp::socket socket(
        io_context, udp::endpoint(udp::v4(), std::stoi(listen_port)));
    udp::endpoint sender_endpoint;
    std::array<char, 1024> recv_buffer;

    while (true) {
        boost::system::error_code error;
        size_t len = socket.receive_from(
            boost::asio::buffer(recv_buffer), sender_endpoint, 0, error);

        if (error && error != boost::asio::error::message_size) {
            throw boost::system::system_error(error);
        }

        std::string message(recv_buffer.data(), len);

        // Prepare a Kafka message
        ProducerRecord record(
            topic, NullKey, Value(message.c_str(), message.size()));

        // Prepare delivery callback
        auto deliveryCb
            = [message](const RecordMetadata& metadata, const Error& error) {
                  if (!error) {
                      std::cout << "Message delivered: " << metadata.toString()
                                << std::endl;
                  } else {
                      std::cerr << "Message failed to be delivered: "
                                << error.message() << std::endl;
                  }
              };

        // Send the message to Kafka
        producer.send(record, deliveryCb);
    }

    // Close the producer explicitly (or not, since RAII will take care of it)
    producer.close();
}

int main(int argc, char* argv[])
{
    std::string listen_port;
    std::string produce_ip;
    std::string produce_port;
    std::string topic;

    // Parse command line arguments
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")("listen.port",
        po::value<std::string>(&listen_port)->required(),
        "Port to listen for UDP messages")("produce.ip",
        po::value<std::string>(&produce_ip)->required(),
        "IP address of the Kafka broker")("produce.port",
        po::value<std::string>(&produce_port)->required(),
        "Port of the Kafka broker")("topic",
        po::value<std::string>(&topic)->required(),
        "Kafka topic to produce messages to");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << desc << "\n";
        return 1;
    }

    try {
        run_bridge(listen_port, produce_ip, produce_port, topic);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}