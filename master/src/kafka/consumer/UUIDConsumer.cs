using Confluent.Kafka;
using System;
using System.Threading;
using System.Threading.Tasks;
class UUIDConsumerService : IDisposable
{
    private ConsumerConfig _config;
    private Master _master;
    private string _topic;

    public UUIDConsumerService(Config config, string topic)
    {
        _master = Master.GetInstance();
        var configObject = _master._setupConfig.GetYamlObjectConfig();

        var kafkaConfig = configObject["kafka_brokers"];
        string kafka_brokers = kafkaConfig.ToString();
        _config = new ConsumerConfig
        {
            BootstrapServers = kafka_brokers,
            GroupId = "UUID",
            AutoOffsetReset = AutoOffsetReset.Earliest
        };

        _topic = topic;
        _master = Master.GetInstance();
    }

public async Task<int> GetUUIDCountAsync()
{
    int totalMessages = 0;

    try
    {
        using (var consumer = new ConsumerBuilder<Ignore, Ignore>(_config).Build())
        {
            Console.WriteLine($"topic: {_topic}");
            consumer.Subscribe(_topic); // Subscribe to the topic

            await Task.Delay(1000); // Give some time for partitions to be assigned

            var partitions = consumer.Assignment;
            Console.WriteLine($"Partitions: {partitions.Count}");
            if (partitions.Count == 0)
            {
                partitions = consumer.Assignment;
                consumer.Assign(partitions);
            }

            foreach (var partition in partitions)
            {
                var watermarkOffsets = consumer.QueryWatermarkOffsets(new TopicPartition(_topic, partition.Partition), TimeSpan.FromSeconds(10));
                Console.WriteLine($"Watermark offsets for partition {partition.Partition}: {watermarkOffsets.Low} - {watermarkOffsets.High}");
                var earliestOffset = watermarkOffsets.Low;
                Console.WriteLine($"Earliest offset: {earliestOffset}");
                var latestOffset = watermarkOffsets.High;
                Console.WriteLine($"Latest offset: {latestOffset}");

                int messageCount = (int)(latestOffset - earliestOffset);
                totalMessages += messageCount;
            }
        }
    }
    catch (Exception e)
    {
        Console.WriteLine($"Error counting UUIDs: {e.Message}");
    }

    return totalMessages;
}


    ~UUIDConsumerService()
    {
        Dispose();
    }

    public void Dispose()
    {
        Console.WriteLine("Kafka consumer disposed.");
    }
}