using Confluent.Kafka;
using System;
using System.Threading;
using System.Threading.Tasks;

class UUIDProducerService : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly string _topic;
    private bool _disposed = false;
    private Master _master = Master.GetInstance();
    public UUIDProducerService(string topic)
    {
        Console.WriteLine("UUIDProducerService initialized.");
        _topic = topic;
        try
        {
            var configObject = _master._setupConfig.GetYamlObjectConfig();
            var kafkaConfig = configObject["kafka_brokers"];
            string kafka_brokers = kafkaConfig.ToString();

            _config = new ProducerConfig
            {
                BootstrapServers = kafka_brokers,
                Acks = Acks.All
            };
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
        }
    }

    public async Task ProduceUUID(int count = 1)
    {
        for (int i = 0; i < count; i++)
        {
            string uuid = Guid.NewGuid().ToString();

            _master.kFKProducer.SendMessageAsync(_topic, uuid);
        }
    }

    protected virtual void Dispose(bool disposing)
    {
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
