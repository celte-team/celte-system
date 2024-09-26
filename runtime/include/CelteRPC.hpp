#pragma once
#include "kafka/Header.h"
#include "kafka/KafkaConsumer.h"
#include "kafka/KafkaProducer.h"
#include "kafka/Properties.h"
#include "kafka/Types.h"
#include "topics.hpp"
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <functional>
#include <future>
#include <memory>
#include <msgpack.hpp>
#include <optional>
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
 * @brief Deserializes the string to the values passed by reference.
 */
template <typename... Args>
void unpack(const std::string &data, Args &...dump) {
  msgpack::object_handle oh = msgpack::unpack(data.data(), data.size());
  msgpack::type::tuple<Args...> dst;
  oh.get().convert(dst);
  std::tie(dump...) = dst;
}

/**
 * @brief Invokes the selected callable by unserializing the arguments to
 * the Args types. Returns Ret value.
 */
template <typename Ret, typename Callable, typename... Args>
Ret __invoke__(Callable callable, std::string serializedArguments) {
  // Deserialize the arguments
  std::stringstream buffer(serializedArguments);
  std::tuple<Args...> args;
  msgpack::object_handle oh =
      msgpack::unpack(buffer.str().data(), buffer.str().size());
  msgpack::object deserialized = oh.get();
  // Convert the deserialized object to a tuple of the expected arguments
  deserialized.convert(args);

  // Invoke the RPC
  return std::apply(callable, args);
}

/**
 * @brief Packs all the arguments into a binary string using msgpack.
 */
template <typename... Args>
std::shared_ptr<std::string> __serialize__(Args... args) {
  msgpack::type::tuple<Args...> arguments(args...);

  std::stringstream buffer;
  msgpack::pack(buffer, arguments);
  return std::make_shared<std::string>(buffer.str());
}

/**
 * @brief Returns the value of a header with the given key from a consumer
 * record, or throws a runtime exception if no header is found with the fiven
 * key.
 */
std::string
__getHdrValue__(const kafka::clients::consumer::ConsumerRecord &record,
                const std::string &key);

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
  using RemoteProcedure =
      std::function<void(kafka::clients::consumer::ConsumerRecord,
                         std::string serializedArguments)>;

  enum Scope {
    PEER, // targets a specific peer from its uuid
    CHUNK,
    GRAPPE, // not implemented yet
    GLOBAL  // not implemented yet
  };

  struct RPCBucket {
    // The actual callback to be invoked upon receiving a message
    RemoteProcedure call = [](kafka::clients::consumer::ConsumerRecord,
                              std::string serializedArguments) {};
    // The scope of the RPC, to check validity of the scope before invoking
    Scope scope = Scope::GLOBAL;
    // This flag is set to true if the RPC can return a result.
    bool hasReturnValue = false;
  };

  Table();

  /**
   * @brief Registers a remote procedure by name, associating a callback to be
   * invoked upon receiving a message with the given name.
   * This only registers the methods for executing locally, so it shall be
   * invoked by remote peers.
   */
  template <typename... Args>
  void Register(std::string name, std::function<void(Args...)> rpc,
                Scope scope = Scope::GLOBAL) {
    // Create the callback to be invoked upon receiving a message
    RemoteProcedure call = [rpc](kafka::clients::consumer::ConsumerRecord,
                                 std::string serializedArguments) {
      try {
        __invoke__<void, decltype(rpc), Args...>(rpc, serializedArguments);
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
   * @brief Registers an rpc that can return a value.
   * The return value is published to the .rpc topic of the peer that sent the
   * rpc. There is no rpc name associated to it, instead it has the key value
   * {'answer': rpcUUID} where rpcUUID is the uuid of the rpc that was sent.
   * The scope cannot be something else that PEER.
   */
  /**
   * @brief Registers an rpc that can return a value.
   * The return value is published to the .rpc topic of the peer that sent the
   * rpc. There is no rpc name associated to it, instead it has the key value
   * {'answer': rpcUUID} where rpcUUID is the uuid of the rpc that was sent.
   * The scope cannot be something else that PEER.
   */
  template <typename Ret, typename... Args>
  void RegisterAwaitable(std::string name, std::function<Ret(Args...)> rpc) {
    // Create the callback to be invoked upon receiving a message
    RemoteProcedure call =
        [this, rpc](kafka::clients::consumer::ConsumerRecord consumerRecord,
                    std::string serializedArguments) {
          try {
            auto result = __invokeRPC<Ret, Args...>(rpc, serializedArguments);
            auto rpcUUID = __getHdrValueSharedPtr(consumerRecord,
                                                  celte::tp::HEADER_RPC_UUID);
            auto peerUUID = __getHdrValueSharedPtr(consumerRecord,
                                                   celte::tp::HEADER_PEER_UUID);
            auto record =
                __createResultProducerRecord(*peerUUID, *rpcUUID, result);

            sendRecord(record, rpcUUID, peerUUID, result);

          } catch (const msgpack::type_error &e) {
            __handleDeserializationError(e);
          } catch (const std::exception &e) {
            __handleException(e);
          }
        };

    rpcs.insert(std::make_pair(name, RPCBucket{call, Scope::PEER, true}));
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
    InvokeByTopic(peerId + "." + celte::tp::RPCs, rpName, args...);
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
    InvokeByTopic(chunkId + "." + celte::tp::RPCs, rpName, args...);
  }

  /**
   * @brief Invokes a remote procedure by name for the given grape.
   */
  template <typename... Args>
  void InvokeGrape(const std::string &grapeId, const std::string &rpName,
                   Args... args) {
    InvokeByTopic(grapeId + "." + celte::tp::RPCs, rpName, args...);
  }

  /**
   * @brief Invokes a remote procedure by name for the global scope.
   */
  template <typename... Args>
  void InvokeGlobal(const std::string &name, Args... args) {
    InvokeByTopic(celte::tp::GLOBAL + "." + celte::tp::RPCs, name, args...);
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
    auto serializedArguments = __serialize__(args...);

    auto record = kafka::clients::producer::ProducerRecord(
        topic, kafka::NullKey,
        kafka::Value(serializedArguments->data(), serializedArguments->size()));

    std::shared_ptr<std::string> rpcUUID = std::make_shared<std::string>(
        boost::uuids::to_string(boost::uuids::random_generator()()));

    // Set the headers of the record to hold the name of the remote procedure
    auto rpNamePtr = std::make_shared<std::string>(rpName);
    record.headers() = {
        {kafka::Header{
            kafka::Header::Key{"rpcUUID"},
            kafka::Header::Value{rpcUUID->c_str(), rpcUUID->size()}}},
        {kafka::Header{
            kafka::Header::Key{"rpName"},
            kafka::Header::Value{rpNamePtr->c_str(), rpNamePtr->size()}}}};

    // we capture the serialized arguments to avoid a dangling pointer until
    // the message is sent
    auto deliveryCb =
        [serializedArguments, rpNamePtr,
         rpcUUID](const kafka::clients::producer::RecordMetadata &metadata,
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

  template <typename... Args>
  std::future<std::string> Call(const std::string &peerName,
                                const std::string rpName, Args... args) {
    auto serializedArguments = __serialize__(args...);
    std::shared_ptr<std::string> rpcUUID = std::make_shared<std::string>(
        boost::uuids::to_string(boost::uuids::random_generator()()));

    auto record = kafka::clients::producer::ProducerRecord(
        peerName + "." + celte::tp::RPCs, kafka::NullKey,
        kafka::Value(serializedArguments->data(), serializedArguments->size()));
    record.headers() = {
        {kafka::Header{
            kafka::Header::Key{"rpcUUID"},
            kafka::Header::Value{rpcUUID->c_str(), rpcUUID->size()}}},
        {kafka::Header{kafka::Header::Key{"rpName"},
                       kafka::Header::Value{rpName.c_str(), rpName.size()}}}};

    rpcPromises.emplace(*rpcUUID,
                        std::make_shared<std::promise<std::string>>());
    std::future<std::string> future = rpcPromises.at(*rpcUUID)->get_future();

    auto deliveryCb = [rpcUUID, serializedArguments](
                          const kafka::clients::producer::RecordMetadata &,
                          const kafka::Error &error) {
      if (error) {
        std::cerr << "An error occured (RPC call): " << error.message()
                  << std::endl;
        std::cerr << "Failed to send message: " << *rpcUUID << std::endl;
      }
    };

    __send(record, deliveryCb);
    return future;
  }

  void ForgetRPC(const std::string &name) { rpcs.erase(name); }

private:
  std::unordered_map<std::string, RPCBucket> rpcs;

  /**
   * @brief Invokes an rpc using the serialized arguments passed as a string,
   * and returns the serialized result as a shared pointer.
   *
   * @param rpc the lambda expression to invoke
   * @param serializedArguments the binary data of all the arguments to pass to
   * the lambda, packed as a string.
   *
   * @return std::shared_ptr<std::string> the result serialized as a string. The
   * shared pointer is used to send the result over kafka whilst avoiding memory
   * leaks.
   */
  template <typename Ret, typename... Args>
  std::shared_ptr<std::string>
  __invokeRPC(std::function<Ret(Args...)> rpc,
              const std::string &serializedArguments) {
    return __serialize__(
        __invoke__<Ret, decltype(rpc), Args...>(rpc, serializedArguments));
  }

  /**
   * @brief Retruns the header value for the given key of a consumer record,
   * and wraps it into a shared pointer to be sent over kafka.
   */
  inline std::shared_ptr<std::string>
  __getHdrValueSharedPtr(const kafka::clients::consumer::ConsumerRecord &record,
                         const std::string &key) {
    return std::make_shared<std::string>(__getHdrValue__(record, key));
  }

  /**
   * @brief Creates a producer record to be used to send the result of an rpc.
   *
   * @param peerUUID : the UUID of the peer the response shall be sent too.
   * @param rpcUUID : the UUID corresponding to the request that we are
   * responding to.
   */
  kafka::clients::producer::ProducerRecord __createResultProducerRecord(
      const std::string &peerUUID, const std::string &rpcUUID,
      const std::shared_ptr<std::string> &serializedResult) {
    auto record = kafka::clients::producer::ProducerRecord(
        peerUUID + "." + celte::tp::RPCs, kafka::NullKey,
        kafka::Value(serializedResult->data(), serializedResult->size()));

    record.headers() = {
        {kafka::Header{kafka::Header::Key{"answer"},
                       kafka::Header::Value{rpcUUID.c_str(), rpcUUID.size()}}}};
    return record;
  }

  /**
   * @brief Sends a record to kafka to return the result of an rpc call, whilst
   * ensuring that all the relevant data is captured in the delivery callback to
   * avoid memory leaks.
   */
  inline void sendRecord(kafka::clients::producer::ProducerRecord &record,
                         std::shared_ptr<std::string> &rpcUUID,
                         std::shared_ptr<std::string> &peerUUID,
                         std::shared_ptr<std::string> &serializedResult) {
    __send(record, [rpcUUID, peerUUID, serializedResult](
                       const kafka::clients::producer::RecordMetadata &,
                       kafka::Error err) {
      if (err) {
        std::cerr << "Error in RPC return: " << err.message() << std::endl;
      }
    });
  }

  /**
   * @brief Logs the error to stderr.
   */
  inline void __handleDeserializationError(const msgpack::type_error &e) {
    std::cerr << "Type error during deserialization: " << e.what() << std::endl;
  }

  /**
   * @brief Logs the error to stderr.
   */
  inline void __handleException(const std::exception &e) {
    std::cerr << "Exception during deserialization: " << e.what() << std::endl;
  }

  /**
   * @brief Sends a record for invoking an rpc.
   */
  void __send(
      kafka::clients::producer::ProducerRecord &record,
      const std::function<void(const kafka::clients::producer::RecordMetadata &,
                               kafka::Error)> &onDelivered);

  /**
   * @brief Tries to call an rpc locally (invoked by a remote peer) from its
   * name. If no such rpc is found, a log is produced to stderr and this method
   * returns.
   */
  void __tryInvokeRPC(kafka::clients::consumer::ConsumerRecord record,
                      const std::string &rpName);

  /**
   * @brief This method is called by InvokeLocal when the message
   * does not contain an rpc but instead contains the return value of a
   * previously invoked rpc.
   */
  void __handleRPCReturnedValue(kafka::clients::consumer::ConsumerRecord record,
                                const std::string &rpcUUId);

  // This map holds the promises of the RPCs that are waiting for a return
  // value. When the InvokeLocal method is invoked on a return value, the
  // corresponding promise is set with the return value.
  std::unordered_map<std::string, std::shared_ptr<std::promise<std::string>>>
      rpcPromises;
};

} // namespace rpc
} // namespace celte