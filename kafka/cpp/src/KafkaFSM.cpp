#include "KafkaFSM.hpp"
#include "KafkaLinkStatesDeclaration.hpp"
#include "kafka/Properties.h"
#include <kafka/AdminClient.h>

namespace tinyfsm {
    template <> void Fsm<celte::nl::AKafkaLink>::set_initial_state()
    {
        Fsm<celte::nl::AKafkaLink>::current_state_ptr
            = &_state_instance<celte::nl::states::KLDisconnected>::value;
    }
} // namespace tinyfsm

using namespace celte::nl;

std::unordered_map<std::string, AKafkaLink::Consumer> AKafkaLink::_consumers
    = {};

kafka::Properties AKafkaLink::kDefaultProps = kafka::Properties(
    kafka::Properties::PropertiesMap({ { "enable.idempotence", { "true" } }}));

AKafkaLink::KCelteConfig AKafkaLink::kCelteConfig { .pollingIntervalMs
    = std::chrono::milliseconds(5),
    .pollTimeout = std::chrono::milliseconds(10) };

void AKafkaLink::RegisterConsumer(const std::string& topic,
    AKafkaLink::ScheduledKCTask callback,
    AKafkaLink::CustomPropsUpdater propsUpdater)
{
    auto props = AKafkaLink::kDefaultProps;
    if (propsUpdater)
        propsUpdater(props);

    Consumer consumer = {
        .consumer = std::shared_ptr<kafka::clients::consumer::KafkaConsumer>(
            new kafka::clients::consumer::KafkaConsumer(props)),
        .dataHandler = (callback != nullptr) ? callback : [](auto) {},
    };

    consumer.consumer->subscribe({ topic });
}

void AKafkaLink::UnregisterConsumer(const std::string& topic)
{
    if (AKafkaLink::_consumers.find(topic) == AKafkaLink::_consumers.end())
        return;
    AKafkaLink::_consumers[topic].consumer->unsubscribe();
    _consumers.erase(topic);
}

void AKafkaLink::__pollAllConsumers()
{
    for (auto& [topic, consumer] : _consumers) {
        auto records = consumer.consumer->poll(kCelteConfig.pollTimeout);
        if (records.empty())
            continue;
        {
            std::lock_guard<std::mutex> lock(*consumer.mutex);
            consumer.recvdData.push(records);
        }
    }
}

void AKafkaLink::Catchback()
{
    for (auto& [topic, consumer] : _consumers) {
        std::lock_guard<std::mutex> lock(*consumer.mutex);
        while (!consumer.recvdData.empty()) {
            auto records = consumer.recvdData.front();
            consumer.recvdData.pop();
            for (auto& record : records)
                consumer.dataHandler(record);
        }
    }
}

void AKafkaLink::ClearAllConsumers()
{
    for (auto& [topic, consumer] : _consumers) {
        consumer.consumer->unsubscribe();
    }
    _consumers.clear();
}

void AKafkaLink::CreateTopicIfNotExists(const std::string& topic, int numPartitions, int replicaFactor)
{
    kafka::clients::admin::AdminClient adminClient(kDefaultProps);

    auto topics = adminClient.listTopics();
    for (auto& topicName : topics.topics) {
        if (topicName == topic)
            return;
    }

    auto createResult = adminClient.createTopics({topic}, numPartitions, replicaFactor);
    if (!createResult.error || createResult.error.value() == RD_KAFKA_RESP_ERR_TOPIC_ALREADY_EXISTS)
    {
        return;
    }
}
