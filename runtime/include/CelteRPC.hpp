#pragma once
#include "kafka/Header.h"
#include "kafka/KafkaConsumer.h"
#include "kafka/KafkaProducer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"
#include <functional>
#include <memory>
#include <msgpack.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define REGISTER_RPC(name, scope, ...)                                         \
  RUNTIME.RPCTable().Register(                                                 \
      #name, std::function<void(__VA_ARGS__)>([this](auto &&...args) {         \
        name(std::forward<decltype(args)>(args)...);                           \
      }),                                                                      \
      scope)

#define UNREGISTER_RPC(name) RUNTIME.RPCTable().ForgetRPC(#name)
namespace celte {
namespace rpc {

/**
 * @brief A table of remote procedures that can be invoked by name.
 * The invoke method takes a topic and a list of arguments, serializes them
 * and sends them to the specified topic.
 *
 * @warning This is not thread safe!
 *
 */
class Table {
public:
  using RemoteProcedure = std::function<void(std::string serializedArguments)>;

  enum Scope {
    PEER, // targets a specific peer from its uuid
    CHUNK,
    GRAPPE, // not implemented yet
    GLOBAL  // not implemented yet
  };

  struct RPCBucket {
    // The actual callback to be invoked upon receiving a message
    RemoteProcedure call;
    // The scope of the RPC, to check validity of the scope before invoking
    Scope scope;
  };

  Table();

  template <typename... Args>
  void Register(std::string name, std::function<void(Args...)> rpc,
                Scope scope = Scope::GLOBAL) {
    // Create the callback to be invoked upon receiving a message
    RemoteProcedure call = [rpc](std::string serializedArguments) {
      try {
        // Deserialize the arguments
        std::stringstream buffer(serializedArguments);
        std::tuple<Args...> args;
        msgpack::object_handle oh =
            msgpack::unpack(buffer.str().data(), buffer.str().size());
        msgpack::object deserialized = oh.get();
        // Convert the deserialized object to a tuple of the expected arguments
        deserialized.convert(args);

        // Invoke the RPC
        std::apply(rpc, args);
      } catch (const msgpack::type_error &e) {
        std::cerr << "Type error during deserialization: " << e.what()
                  << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "Exception during deserialization: " << e.what()
                  << std::endl;
      }
    };

    rpcs.insert(std::make_pair(name, RPCBucket{call, scope}));
  }

  /**
   * @brief Invokes a remote procedure by name, for the given peer.
   *
   * @param name
   * @param args
   */
  template <typename... Args>
  void InvokePeer(const std::string &peerId, const std::string &rpName,
                  Args... args) {
    InvokeByTopic(peerId + ".rpc", rpName, args...);
  }

  /**
   * @brief Invokes a remote procedure by name, for the given scope.
   *
   * @param name
   * @param args
   */
  template <typename... Args>
  void InvokeChunk(const std::string &chunkId, const std::string &rpName,
                   Args... args) {
    InvokeByTopic(chunkId + ".rpc", rpName, args...);
  }

  /**
   * @brief Invokes a remote procedure by name for the given grape.
   */
  template <typename... Args>
  void InvokeGrape(const std::string &grapeId, const std::string &rpName,
                   Args... args) {
    InvokeByTopic(grapeId + ".rpc", rpName, args...);
  }

  /**
   * @brief Invokes a remote procedure by name for the global scope.
   */
  template <typename... Args>
  void InvokeGlobal(const std::string &name, Args... args) {
    InvokeByTopic("global.rpc", name, args...);
  }

  /**
   * @brief Consumers seeking to execute RPCs should call this method
   * upon recieving a message from the topic they are listening to to
   * execute the RPC.
   */
  void InvokeLocal(kafka::clients::consumer::ConsumerRecord record);

  /**
   * @brief Invokes a remote procedure by serializing the arguments and sending
   * them to the specified topic. The concerned entity should be listening to
   * the topic to receive the message.
   *
   *
   * TODO: Room for improvement maybe, to use Key instead of Header to identity
   * the remote procedure To see if this has any impact on performance.
   */
  template <typename... Args>
  void InvokeByTopic(const std::string &topic, const std::string &rpName,
                     Args... args) {
    // Serialize the arguments
    msgpack::type::tuple<Args...> arguments(args...);
    std::stringstream buffer;
    msgpack::pack(buffer, arguments);
    auto serializedArguments = std::make_shared<std::string>(buffer.str());

    auto record = kafka::clients::producer::ProducerRecord(
        topic, kafka::NullKey,
        kafka::Value(serializedArguments->data(), serializedArguments->size()));

    // Set the headers of the record to hold the name of the remote procedure
    auto rpNamePtr = std::make_shared<std::string>(rpName);
    record.headers() = {{kafka::Header{
        kafka::Header::Key{"rpName"},
        kafka::Header::Value{rpNamePtr->c_str(), rpNamePtr->size()}}}};

    // we capture the serialized arguments to avoid a dangling pointer until
    // the message is sent
    auto deliveryCb =
        [serializedArguments,
         rpNamePtr](const kafka::clients::producer::RecordMetadata &metadata,
                    const kafka::Error &error) {
          if (error) {
            std::cerr << "An error occured (RPC invoke): " << error.message()
                      << std::endl;
            std::cerr << "Failed to send message: " << serializedArguments
                      << std::endl;
          }
        };

    // Send the message
    __send(record, deliveryCb);
  }

  void ForgetRPC(const std::string &name) { rpcs.erase(name); }

private:
  std::unordered_map<std::string, RPCBucket> rpcs;

  void __send(
      const kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDeliveryError);
};

} // namespace rpc
} // namespace celte