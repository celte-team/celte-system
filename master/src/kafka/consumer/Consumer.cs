using Confluent.Kafka;
using System;
using System.Collections.Generic;

class KFKConsumer : IDisposable
{
    private ConsumerConfig _config;

    public KFKConsumer()
    {
        Master master = Master.GetInstance();
        try
        {
            var configObject = master._setupConfig.GetYamlObjectConfig();

            var kafkaConfig = configObject["kafka_brokers"];
            string kafka_brokers = kafkaConfig.ToString();

            _config = new ConsumerConfig
            {
                BootstrapServers = kafka_brokers,
                GroupId = "test-consumer-group",
                AutoOffsetReset = AutoOffsetReset.Earliest
            };

            using (var consumer = new ConsumerBuilder<Ignore, string>(_config).Build())
            {
                consumer.Subscribe("test-topic");

                try
                {
                    while (true)
                    {
                        var consumeResult = consumer.Consume();
                        Console.WriteLine($"Consumed message '{consumeResult.Value}' at: '{consumeResult.TopicPartitionOffset}'.");
                    }
                }
                catch (OperationCanceledException)
                {
                    consumer.Close();
                }
            }
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error: {e.Message}");
        }
    }

    ~KFKConsumer()
    {
        Dispose();
    }

    public void Dispose()
    {
        Console.WriteLine("Kafka consumer disposed.");
    }
}