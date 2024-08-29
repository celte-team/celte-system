/*
** CELTE, 2024
** celte
**
** Team members:
** Eliot Janvier
** Clement Toni
** Ewen Briand
** Laurent Jiang
** Thomas Laprie
**
** File description:
** CelteNL.cpp
*/

#include "CelteNL.hpp"
#include "NLUtils.hpp"
#include <chrono>
#include <iostream>

namespace celte {
    namespace net {
        /* --------------------------------- PUBLIC
         * --------------------------------- */

        CelteNL::CelteNL(CelteNLOptions options)
            : _ioContext()
            , _localEndpoint(boost::asio::ip::udp::v4(), options.port)
            , _socket(_ioContext)
            , _recvBuffer(options.recvBufferSize)
            , _sendBuffer(options.sendBufferSize)
            , _running(false)
            , _options(options)
            , _sendQueueMutex(std::make_shared<boost::mutex>())
            , _connectionsMutex(std::make_shared<boost::mutex>())
            , _connectionTimeout(options.connectionTimeout)
        {
        }

        CelteNL::~CelteNL() { Stop(); }

        void CelteNL::Start()
        {
            _running = true;
            __bindSocket();
            __startTimeoutThread();
            __startSendWorkers();
            __startReceiving();
            std::cout << "Network layer started" << std::endl;
        }

        void CelteNL::Stop()
        {
            if (not _running)
                return;
            _running = false;
            _ioContext.stop();

            _dataQueue.Stop();
            _sendQueueCondVar.notify_all();
            _processingWorkers.join_all();
            _receiveThread.join();
            _timeoutThread.join();
            _sendWorkers.join_all();
            std::cout << "Network layer stopped" << std::endl;
        }

        void CelteNL::Send(boost::asio::ip::udp::endpoint dest, Packet message)
        {
            {
                boost::mutex::scoped_lock lock(*_sendQueueMutex);
                _sendBuffer.push_back({ dest, message });
            }
            _sendQueueCondVar.notify_one();
        }

        void CelteNL::RegisterHandler(int opcode, OperationHandler handler)
        {
            _handlers.insert({ opcode, handler });
        }

        void CelteNL::SetNewConnectionCallback(ConnectionEventHandler&& handler)
        {
            _newConnectionCallback = std::move(handler);
        }

        void CelteNL::SetConnectionLostCallback(
            ConnectionEventHandler&& handler)
        {
            _connectionLostCallback = std::move(handler);
        }

        /* --------------------------------- PRIVATE
         * -------------------------------- */

        void CelteNL::__bindSocket()
        {
            boost::system::error_code error;
            _socket.open(_localEndpoint.protocol(), error);
            if (error)
                throw std::runtime_error("Failed to open socket (1)");
            _socket.set_option(
                boost::asio::ip::udp::socket::reuse_address(true));
            _socket.bind(_localEndpoint, error);
        }

        void CelteNL::__startSendWorkers()
        {
            for (int i = 0; i < _options.maxSendWorkers; i++) {
                _sendWorkers.create_thread([this]() {
                    while (_running) {
                        __sendNextMessage();
                        // avoid busy waiting
                        boost::this_thread::sleep_for(
                            boost::chrono::milliseconds(1));
                    }
                });
            }
        }

        void CelteNL::__startReceiving()
        {
            _receiveThread = boost::thread([this]() {
                try {
                    __callReceive();
                    _ioContext.run();
                } catch (const std::exception& e) {
                    std::cerr << "Exception in worker thread: " << e.what()
                              << std::endl;
                }
            });
            for (int i = 0; i < _options.maxProcessingWorkers; i++) {
                _processingWorkers.create_thread([this]() {
                    while (_running) {
                        if (auto opt = _dataQueue.Pop()) [[likely]] {
                            auto [sender, data] = *opt;
                            Connection& connection
                                = __updateConnectionBuffer(sender, data);
                            __processConnectionData(connection);
                        }
                    }
                });
            }
        }

        void CelteNL::__callReceive()
        {
            if (not _running)
                return;
            _socket.async_receive_from(boost::asio::buffer(_recvBuffer),
                _tempEndpoint,
                [this](const boost::system::error_code& error, size_t bytes) {
                    if (error) {
                        std::cerr << "Error while receiving message: "
                                  << error.message() << std::endl;
                        return;
                    }
                    // we delay processing to avoid blocking the receive thread
                    _dataQueue.Push(std::make_pair(
                        std::move(_tempEndpoint), std::move(_recvBuffer)));
                    _recvBuffer
                        = std::vector<unsigned char>(_options.recvBufferSize);
                    __callReceive();
                });
        }

        Connection& CelteNL::__updateConnectionBuffer(
            boost::asio::ip::udp::endpoint sender,
            std::vector<unsigned char> data)
        {
            std::string senderStr = utils::endpointToString(sender);
            if (_connections.find(senderStr) == _connections.end()) {
                boost::mutex::scoped_lock lock(*_connectionsMutex);
                _connections.insert(
                    { senderStr, Connection(sender, _options.recvBufferSize) });
                if (_newConnectionCallback) {
                    (*_newConnectionCallback)(_connections.at(senderStr));
                }
            }

            _connections.at(senderStr).AppendBytes(data);
            return _connections.at(senderStr);
        }

        void CelteNL::__processConnectionData(Connection& connection)
        {
            std::optional<Packet> message;

            while (message = connection.GetNextMessage()) {
                if (_handlers.find(message->_header.opcode)
                    != _handlers.end()) {
                    _handlers.at(message->_header.opcode)(
                        connection.GetEndpoint(), *message);
                }
            }
        }

        void CelteNL::__sendNextMessage()
        {
            boost::asio::ip::udp::endpoint dest;
            std::vector<unsigned char> data;
            {
                if (not _running)
                    return;
                boost::mutex::scoped_lock lock(*_sendQueueMutex);
                _sendQueueCondVar.wait(
                    lock, [this] { return !_sendBuffer.empty() || !_running; });

                if (not _running)
                    return;
                auto message = _sendBuffer.front();
                dest = std::move(message.first);
                data = message.second.Serialize();
                _sendBuffer.pop_front();
                _socket.send_to(boost::asio::buffer(data), dest);
            }
        }

        void CelteNL::__startTimeoutThread()
        {
            _timeoutThread = boost::thread([this]() {
                while (_running) {
                    if (_connectionTimeout <= 0) {
                        return;
                    }
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(_connectionTimeout));
                    auto now = std::chrono::system_clock::now();
                    auto timedOut = std::vector<std::string>();
                    // Check if any connection timed out
                    for (auto& [endpointStr, connection] : _connections) {
                        if (std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                now - connection.GetLastSeen())
                                .count()
                            > _connectionTimeout) {
                            timedOut.push_back(endpointStr);
                        }
                    }
                    // Remove timed out connections
                    for (auto& endpointStr : timedOut) {
                        if (_connectionLostCallback) {
                            (*_connectionLostCallback)(
                                _connections.at(endpointStr));
                        }
                        {
                            boost::mutex::scoped_lock lock(*_connectionsMutex);
                            _connections.erase(endpointStr);
                        }
                        // wait a bit to avoid busy waiting
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                    }
                }
            });
        }

    } // namespace net
} // namespace celte