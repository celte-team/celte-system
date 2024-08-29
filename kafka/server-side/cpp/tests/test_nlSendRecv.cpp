#include "CelteNL.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <future>
#include <gtest/gtest.h>
#include <thread>

TEST(nlSendRecv, basicServerClientInteraction)
{
    // Vars used for testing
    std::atomic_bool serverReceived = false;
    std::atomic_int recvValue = 0;

    // Creating a server
    celte::net::CelteNLOptions serverOptions { .port = 12345 };
    celte::net::CelteNL server(serverOptions);
    server.RegisterHandler(0,
        [&serverReceived, &recvValue](
            boost::asio::ip::udp::endpoint edp, celte::net::Packet p) {
            serverReceived = true;

            int value = 0;
            p >> value;
            recvValue = value;
        });
    server.Start();

    // Creating a client
    celte::net::CelteNLOptions clientOptions { .port = 12346 };
    celte::net::CelteNL client(clientOptions);
    boost::asio::ip::udp::endpoint serverEndpoint(
        boost::asio::ip::address::from_string("0.0.0.0"), serverOptions.port);
    celte::net::Packet packet(0);
    packet << 42;

    client.Start();

    // wait a few ms to let the server and client start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Send a packet from the client to the server
    client.Send(serverEndpoint, packet);

    // wait a few ms to let the server receive the packet
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Check if the server received the packet
    ASSERT_TRUE(serverReceived);
    ASSERT_EQ(recvValue, 42);

    // Stop the server and client
    server.Stop();
    client.Stop();

    // wait for the server and client to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

/**
 * @brief Test if the connection callbacks are called when a connection is
 * established or closed (times out)
 *
 */
TEST(nlConnectionCallbacks, basicConnectionCallbacks)
{
    // Vars used for testing
    std::atomic_int newConnectionCount = 0;
    std::atomic_int connectionLostCount = 0;
    std::atomic_int messageReceivedCount = 0;

    // Test parameters
    int clientCount = 10;

    // Creating server
    celte::net::CelteNLOptions serverOptions {
        .connectionTimeout = 100,
        .port = 2345,
    };
    celte::net::CelteNL server(serverOptions);
    server.SetNewConnectionCallback(
        [&newConnectionCount](celte::net::Connection& c) {
            std::cout << "new connection at endpoint "
                      << c.GetEndpoint().address().to_string() << ":"
                      << c.GetEndpoint().port() << std::endl;
            newConnectionCount++;
        });
    server.SetConnectionLostCallback(
        [&connectionLostCount](
            celte::net::Connection& c) { connectionLostCount++; });
    server.RegisterHandler(0,
        [&messageReceivedCount](
            boost::asio::ip::udp::endpoint edp, celte::net::Packet p) {
            std::cout << "handler called" << std::endl;
            messageReceivedCount++;
        });
    std::cout << "Server starting..." << std::endl;
    server.Start();
    std::cout << "Server Started" << std::endl;

    // Creating clients
    std::vector<std::unique_ptr<celte::net::CelteNL>> clients;
    for (int i = 0; i < clientCount; i++) {
        celte::net::CelteNLOptions clientOptions { .port = 2300 + i };
        clients.push_back(std::make_unique<celte::net::CelteNL>(clientOptions));
        clients.back()->Start();
    }

    // Wait a few ms to let the server and clients start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Send a packet from each client to the server
    for (int i = 0; i < clientCount; i++) {
        boost::asio::ip::udp::endpoint serverEndpoint(
            boost::asio::ip::address::from_string("0.0.0.0"),
            serverOptions.port);
        celte::net::Packet packet(0);
        packet << 42;
        clients[i]->Send(serverEndpoint, packet);
    }

    // wait a few ms to let the server receive the packets
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if the server received the packets
    ASSERT_EQ(newConnectionCount, clientCount);
    ASSERT_EQ(messageReceivedCount, clientCount);

    // Wait for the connections to time out
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check if the connections timed out
    ASSERT_EQ(connectionLostCount, clientCount);

    // Stop the server and clients
    std::cout << "Stopping server and clients" << std::endl;
    server.Stop();
    for (int i = 0; i < clientCount; i++) {
        clients[i]->Stop();
    }

    std::cout << "Server and clients stopped" << std::endl;
}