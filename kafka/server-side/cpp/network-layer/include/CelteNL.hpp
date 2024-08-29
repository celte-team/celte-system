/*
** CELTE, 2024
** cpp
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** CelteNL.hpp
*/

#pragma once
#include "Connection.hpp"
#include "Packet.hpp"
#include "ThreadSafeQueue.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/thread.hpp>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace celte {
    namespace net {
        /**
         * @brief Options for constructing a CelteNL object.
         * Options are:
         * - recvBufferSize: size of the buffer used to receive messages, in
         * bytes
         * - sendBufferSize: size of the buffer used to store messages to send,
         * in messages
         * - maxSendWorkers: number of threads used to send messages
         * - maxProcessingWorkers: number of threads used to process messages
         * - connectionBufferSize: size of the buffer used to store data from
         * a single connection, in bytes
         * - port: port to bind the socket to (0 to let the OS choose a port)
         * - connectionTimeout: time in milliseconds after which a connection is
         * considered lost if no new data is received (-1 to disable)
         *
         */
        struct CelteNLOptions {
            size_t recvBufferSize = 4048;
            size_t sendBufferSize = 1024;
            size_t maxSendWorkers = 4;
            size_t maxProcessingWorkers = 4;
            size_t connectionBufferSize = 1024;
            int connectionTimeout = -1;
            int port;
        };

        /**
         * @brief Network layer with the following features:
         * - user can register handlers for specific opcodes
         * - user can register callbacks for new connections and lost
         * connections
         * - user can set a timeout after which a connection is considered lost
         * - user can send messages to specific endpoints
         * - user can start and stop the network layer
         *
         * The following threads are running in the background:
         * - _receiveThread: responsible for receiving messages from the socket
         * and pushing them to a queue where they wait to be handled
         * - _timeoutThread: responsible for checking if any connection timed
         * out
         * - _sendWorkers: a group of threads responsible for sending messages
         * from the send buffer
         * - _processingThread: thread group responsible for processing messages
         * from the queue
         *
         * This class is initialized with a set of options that can be modified
         * at any time. See CelteNLOptions for more information.
         *
         */
        class CelteNL {
        public:
            using OperationHandler
                = std::function<void(boost::asio::ip::udp::endpoint, Packet)>;
            using ConnectionEventHandler = std::function<void(Connection&)>;

            CelteNL(CelteNLOptions options);
            ~CelteNL();

            /**
             * @brief Starts the network layer. This will bind the socket to the
             * local endpoint and start the receiving thread.
             *
             */
            void Start();

            /**
             * @brief Stops all services and threads.
             *
             */
            void Stop();

            /**
             * @brief Schedules a message to be sent to the specified endpoint.
             *
             */
            void Send(boost::asio::ip::udp::endpoint, Packet);

            /**
             * @brief Registers a callback to be called
             * when a message with the specified opcode is received.
             *
             */
            void RegisterHandler(int, OperationHandler);

            /**
             * @brief Sets the callback to be called when a new connection is
             * established.
             *
             */
            void SetNewConnectionCallback(ConnectionEventHandler&&);

            /**
             * @brief Sets the callback to be called when a connection is lost.
             *
             */
            void SetConnectionLostCallback(ConnectionEventHandler&&);

        private:
            /* ---------------------------------- INIT
             * ---------------------------------- */
            /**
             * @brief Binds the socket to the local endpoint.
             */
            void __bindSocket();

            /**
             * @brief Starts a thread group that will send messages from the
             * send buffer.
             */
            void __startSendWorkers();

            /**
             * @brief Starts listening for incoming messages. Any complete
             * message will be parsed and sent to the appropriate handler.
             *
             */
            void __startReceiving();

            /**
             * @brief Restarts the async callback that receives data from the
             * socket.
             *
             */
            void __callReceive();

            /**
             * @brief Sends one message from the send buffer or waits
             * until a message is available.
             *
             */
            void __sendNextMessage();

            /**
             * @brief While there is data available in the connection's buffer,
             * calls the appropriate handler with the data.
             *
             * @param connection
             * @param senderStr
             */
            void __processConnectionData(Connection& connection);

            /**
             * @brief Creates a new connection if it the associated endpoint
             * is unknown and adds the data to the connection's buffer.
             * Updates the connection's last_seen timestamp.
             *
             * @param sender
             * @param data
             */
            Connection& __updateConnectionBuffer(
                boost::asio::ip::udp::endpoint sender,
                std::vector<unsigned char> data);

            /**
             * @brief Starts a thread responsible for checking if any connection
             * timed out.
             *
             */
            void __startTimeoutThread();

            // map that holds all known connections indexed by their endpoint as
            // a string
            std::map<std::string, Connection> _connections;

            // mutex to protect connections
            std::shared_ptr<boost::mutex> _connectionsMutex;

            // thread responsible for receiving messages
            boost::thread _receiveThread;

            // thread responsible for checking if any connection timed out
            boost::thread _timeoutThread;

            // map that holds all instruction handlers
            std::map<int, OperationHandler> _handlers;

            // The local endpoint to bind the socket to
            boost::asio::ip::udp::endpoint _localEndpoint;

            // The io service context, used to run the io service
            boost::asio::io_context _ioContext;

            // The socket used to send and receive messages
            boost::asio::ip::udp::socket _socket;

            // The buffer used to store incoming messages
            std::vector<unsigned char> _recvBuffer;

            // Temporary endpoint used to store the sender of received messages
            boost::asio::ip::udp::endpoint _tempEndpoint;

            // Update this using Stop() and Start()
            std::atomic<bool> _running;

            // Queue of messages to send
            boost::circular_buffer<
                std::pair<boost::asio::ip::udp::endpoint, Packet>>
                _sendBuffer;
            std::shared_ptr<boost::mutex> _sendQueueMutex;
            boost::condition_variable _sendQueueCondVar;

            // Threads that sends messages from the queue continuously
            boost::thread_group _sendWorkers;

            // Future holding the ioservice run result
            std::future<void> _ioFuture;

            // Options for the network layer kept here for future reference
            CelteNLOptions _options;

            // Callback to be called when a new connection is created
            std::optional<ConnectionEventHandler> _newConnectionCallback;

            // Callback to be called when a connection is lost
            std::optional<ConnectionEventHandler> _connectionLostCallback;

            // Time in milliseconds after which a connection is considered lost
            // if no new data is received
            int _connectionTimeout;

            // Buffer to store data received from the socket before it is
            // processed
            utils::ThreadSafeQueue<std::pair<boost::asio::ip::udp::endpoint,
                std::vector<unsigned char>>>
                _dataQueue;
            boost::thread_group _processingWorkers;
        };
    } // namespace net
} // namespace celte