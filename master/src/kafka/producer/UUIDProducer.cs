using Confluent.Kafka;
using System;
using System.Threading;
using System.Threading.Tasks;

class UUIDProducerService : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly IProducer<Null, string> _producer;
    private readonly string _topic;
    private bool _disposed = false;
    private readonly int _minUUIDs = 100;
    private readonly int _checkIntervalMilliseconds = 30000; // 30 seconds
    private UUIDConsumerService _uuidConsumerService;
    public UUIDProducerService(string topic)
    {
        Master master = Master.GetInstance();
        _topic = topic;
        try
        {
            var configObject = master._setupConfig.GetYamlObjectConfig();
            var kafkaConfig = configObject["kafka_brokers"];
            string kafka_brokers = kafkaConfig.ToString();

            _config = new ProducerConfig
            {
                BootstrapServers = kafka_brokers,
                Acks = Acks.All
            };
            _uuidConsumerService = new UUIDConsumerService(_config, _topic);
            _producer = new ProducerBuilder<Null, string>(_config).Build();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
        }
    }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            try
            {
                int currentUUIDCount = await _uuidConsumerService.GetUUIDCountAsync();
                if (currentUUIDCount < _minUUIDs)
                {
                    int uuidsToProduce = _minUUIDs - currentUUIDCount;
                    Console.WriteLine($"UUID count below threshold. Producing {uuidsToProduce} more UUIDs.");

                    for (int i = 0; i < uuidsToProduce; i++)
                    {
                        string uuid = Guid.NewGuid().ToString();
                        _producer.Produce(_topic, new Message<Null, string> { Value = uuid }, deliveryReport =>
                        {
                            if (deliveryReport.Error.IsError)
                            {
                                Console.WriteLine($"Delivery Error: {deliveryReport.Error.Reason}");
                            }
                            else
                            {
                                Console.WriteLine($"Delivered UUID: {uuid} to {deliveryReport.TopicPartitionOffset}");
                            }
                        });
                    }
                }

                // Wait for the next check
                await Task.Delay(_checkIntervalMilliseconds, cancellationToken);
            }
            catch (ProduceException<Null, string> ex)
            {
                Console.WriteLine($"Failed to produce UUID message: {ex.Error.Reason}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in UUIDProducerService: {ex.Message}");
            }
        }
    }

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

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                // Dispose managed resources
                _producer?.Flush(TimeSpan.FromSeconds(10));
                _producer?.Dispose();
            }
            _disposed = true;
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
        Console.WriteLine("UUIDProducerService disposed.");
    }

    ~UUIDProducerService()
    {
        Dispose(false);
    }
}
