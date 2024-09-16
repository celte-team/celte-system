using Confluent.Kafka;
using System;

class KFKProducer : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly IProducer<Null, string> _producer;
    private bool _disposed = false;
    public UUIDProducerService _uuidProducerService;

    public KFKProducer()
    {
        Master master = Master.GetInstance();
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

            _producer = new ProducerBuilder<Null, string>(_config).Build();
            _uuidProducerService = new UUIDProducerService("UUID");
        }
        catch (Exception e)
        {
        Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
        }
    }

    /// <summary>
    /// Send message without key
    /// </summary>
    public async Task SendMessageAsync(string topic, string message)
    {
        try
        {
            var result = await _producer.ProduceAsync(topic, new Message<Null, string>
            {
                Value = message
            });

            Console.WriteLine($"Message sent to {result.TopicPartitionOffset}");
        }
        catch (ProduceException<Null, string> e)
        {
            Console.WriteLine($"Delivery failed: {e.Error.Reason}");
        }
    }

    /// <summary>
    /// Send message with key
    /// </summary>
    /// <param name="topic"></param>
    /// <param name="key"></param>
    /// <param name="message"></param>
    /// <returns></returns>
    public async Task SendMessageAsync(string topic, string key, string message)
    {
        try
        {
            var producerWithKey = new ProducerBuilder<string, string>(_config).Build();
            var result = await producerWithKey.ProduceAsync(topic, new Message<string, string>
            {
                Key = key,
                Value = message
            });

            Console.WriteLine($"Message sent to {result.TopicPartitionOffset}");
        }
        catch (ProduceException<string, string> e)
        {
            Console.WriteLine($"Delivery failed: {e.Error.Reason}");
        }
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                // Dispose managed resources
            }
            _disposed = true;
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
        Console.WriteLine("Kafka producer disposed.");
    }

    ~KFKProducer()
    {
        Dispose(false);
    }
}
