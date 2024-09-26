#include "CelteRuntime.hpp"
#include "ServerStatesDeclaration.hpp"
#include "topics.hpp"

namespace celte {
    namespace server {
        namespace states {

            void Connecting::entry()
            {
                if (not HOOKS.server.connection.onConnectionProcedureInitiated()) {
                    std::cerr << "Connection procedure hook failed" << std::endl;
                    HOOKS.server.connection.onConnectionError();
                    transit<Disconnected>();
                }

                try {
                    // creating a listener for RPCs related to this server node as a whole
                    std::cout << "Creating RPC listener for " << RUNTIME.GetUUID() << std::endl;
                    KPOOL.Subscribe({ .topic = RUNTIME.GetUUID() + "." + celte::tp::RPCs,
                        .autoCreateTopic = true,
                        .extraProps = { { "auto.offset.reset", "earliest" } },
                        .autoPoll = true,
                        .callback = [this](auto r) {
                            std::cout << "INVOKE LOCAL IN SERVER RPC LISTENER"
                                      << std::endl;
                            RPC.InvokeLocal(r);
                        } });

                    // creating a listener for RPCs related to the server node as a whole
                    std::cout << "Creating RPC listener for " << RUNTIME.GetUUID() << "."
                              << celte::tp::RPCs << std::endl;
                    std::cout << "CREATED LISTENIER FOR RPC CHANNEL "
                              << RUNTIME.GetUUID() + "." + celte::tp::RPCs << std::endl;
                    KPOOL.Subscribe({ .topic = RUNTIME.GetUUID() + "." + celte::tp::RPCs,
                        .autoCreateTopic = true,
                        .extraProps = { { "auto.offset.reset", "earliest" } },
                        .autoPoll = true,
                        .callback = [this](auto r) {
                            std::cout << "INVOKE LOCAL IN SERVER RPC LISTENER"
                                      << std::endl;
                            RPC.InvokeLocal(r);
                        } });

                    std::cout << "Registersing self as " << RUNTIME.GetUUID() << std::endl;
                    KPOOL.Send({
                        .topic = celte::tp::MASTER_HELLO_SN,
                        .value = RUNTIME.GetUUID(),
                        .onDelivered =
                            [this](auto metadata, auto error) {
                                if (error) {
                                    HOOKS.server.connection.onConnectionError();
                                    HOOKS.server.connection.onServerDisconnected();
                                    transit<Disconnected>();
                                } else {
                                    dispatch(EConnectionSuccess());
                                }
                            },
                    });
                } catch (kafka::KafkaException& e) {
                    std::cerr << "Error in Connecting::entry: " << e.what() << std::endl;
                    HOOKS.server.connection.onConnectionError();
                    HOOKS.server.connection.onServerDisconnected();
                    transit<Disconnected>();
                    return;
                }
            }

            void Connecting::exit() { std::cerr << "Exiting StateConnecting" << std::endl; }

            void Connecting::react(EConnectionSuccess const& event)
            {
                // if this fails we cancel the connection. Maybe the user cancelled the
                // connection or something.
                if (not HOOKS.server.connection.onConnectionSuccess()) {
                    std::cerr << "Connection success hook failed" << std::endl;
                    HOOKS.server.connection.onConnectionError();
                    transit<Disconnected>();
                    return;
                }
                transit<Connected>();
            }

        } // namespace states
    } // namespace server
} // namespace celte
