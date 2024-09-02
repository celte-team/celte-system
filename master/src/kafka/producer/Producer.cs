using Confluent.Kafka;
using System;

class KFKProducer : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly IProducer<Null, string> _producer;
    private bool _disposed = false;
    private UUIDProducerService _uuidProducerService;

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
            // use the UUIDProducerService to produce UUIDs to the topic
            _uuidProducerService = new UUIDProducerService("uuids");
            _uuidProducerService.StartAsync();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
        }
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
        Console.WriteLine("Kafka producer disposed.");
    }

    ~KFKProducer()
    {
        Dispose(false);
    }
}
