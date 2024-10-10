using Confluent.Kafka;
using System;
using System.Threading;
using System.Threading.Tasks;
using Confluent.Kafka.Admin;

class UUIDProducerService : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly string _topic;
    private bool _disposed = false;
    private Master _master = Master.GetInstance();

    // store all the UUID that we have produced
    public Dictionary<string, string> _uuids = new Dictionary<string, string>();

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
            _uuids.Add(uuid, uuid);
            _master.kFKProducer.SendMessageAsync(_topic, uuid);
            // OpenTopic(uuid);
        }
    }


    // open topic of each UUID
    public async Task OpenTopic(string uuid)
    {
        var bootstrapServers = _master._setupConfig.GetYamlObjectConfig()["kafka_brokers"].ToString();

        using (var adminClient = new AdminClientBuilder(new AdminClientConfig { BootstrapServers = bootstrapServers }).Build())
        {
            try
            {
                await adminClient.CreateTopicsAsync(new TopicSpecification[] {
                    new TopicSpecification { Name = uuid, NumPartitions = 1, ReplicationFactor = 1 } });

                // subscribe to the topic
            }
            catch (CreateTopicsException e)
            {
                Console.WriteLine($"An error occured creating topic {e.Results[0].Topic}: {e.Results[0].Error.Reason}");
            }
        }
    }

    public void Dispose()
    {
        GC.SuppressFinalize(this);
        Console.WriteLine("UUIDProducerService disposed.");
    }

    ~UUIDProducerService()
    {
        Dispose();
    }
}
