using Confluent.Kafka;
using System;
using System.Collections.Generic;

class KFKProducer : IDisposable
{
    private ProducerConfig _config;
    private IProducer<Null, string> _producer;

    public KFKProducer()
    {
        Master master = Master.GetInstance();
        try {
            var configObject = master._setupConfig.GetYamlObjectConfig();

            var kafkaConfig = configObject["kafka_brokers"];
            string kafka_brokers = kafkaConfig.ToString();

            _config = new ProducerConfig
            {
                BootstrapServers = kafka_brokers,
                Acks = Acks.All,
            };
        } catch (Exception e) {
            Console.WriteLine($"Error: {e.Message}");
        }
    }

    ~KFKProducer()
    {
        Dispose();
    }

    public void Send(string topic, string message)
    {
        try
        {
            _producer.Produce(topic, new Message<Null, string> { Value = message }, deliveryReport =>
            {
                if (deliveryReport.Error.IsError)
                {
                    Console.WriteLine($"Delivery Error: {deliveryReport.Error.Reason}");
                }
                else
                {
                    Console.WriteLine($"Delivered '{deliveryReport.Value}' to '{deliveryReport.TopicPartitionOffset}'");
                }
            });
        }
        catch (ProduceException<Null, string> ex)
        {
            Console.WriteLine($"Failed to produce message: {ex.Error.Reason}");
        }
    }

    public void Dispose()
    {
        _producer?.Flush(TimeSpan.FromSeconds(10));
        _producer?.Dispose();
        Console.WriteLine("Kafka producer disposed.");
    }
}
