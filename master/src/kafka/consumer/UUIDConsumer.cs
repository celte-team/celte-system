    // public async Task<int> GetUUIDCountAsync()
    // {
    //     int totalMessages = 0;

    //     try
    //     {
    //         using (var consumer = new ConsumerBuilder<Ignore, Ignore>(_config).Build())
    //         {
    //             var partitions = consumer.Assignment;

    //             if (partitions.Count == 0)
    //             {
    //                 partitions = consumer.Assignment;
    //                 consumer.Assign(partitions);
    //             }

    //             foreach (var partition in partitions)
    //             {
    //                 var watermarkOffsets = consumer.QueryWatermarkOffsets(new TopicPartition(_topic, partition.Partition), TimeSpan.FromSeconds(10));
    //                 var earliestOffset = watermarkOffsets.Low;
    //                 var latestOffset = watermarkOffsets.High;

    //                 // Calculate the number of messages in this partition
    //                 int messageCount = (int)(latestOffset - earliestOffset);
    //                 totalMessages += messageCount;
    //             }
    //         }
    //     }
    //     catch (Exception e)
    //     {
    //         Console.WriteLine($"Error counting UUIDs: {e.Message}");
    //     }

    //     return totalMessages;
    // }
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
                var partitions = consumer.Assignment;

                if (partitions.Count == 0)
                {
                    partitions = consumer.Assignment;
                    consumer.Assign(partitions);
                }

                foreach (var partition in partitions)
                {
                    var watermarkOffsets = consumer.QueryWatermarkOffsets(new TopicPartition(_topic, partition.Partition), TimeSpan.FromSeconds(10));
                    var earliestOffset = watermarkOffsets.Low;
                    var latestOffset = watermarkOffsets.High;

                    // Calculate the number of messages in this partition
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