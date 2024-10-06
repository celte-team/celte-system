using Confluent.Kafka;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

class KFKProducer : IDisposable
{
    private readonly ProducerConfig _config;
    private readonly IProducer<Null, byte[]> _producer;
    private bool _disposed = false;
    public UUIDProducerService _uuidProducerService;

    private Master master = Master.GetInstance();

    public KFKProducer()
    {
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

            _producer = new ProducerBuilder<Null, byte[]>(_config).Build();
            _uuidProducerService = new UUIDProducerService("UUID");
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
        }
    }

    private async Task ProduceMessageAsync<TKey>(string topic, TKey key, byte[] message, Headers? headers = null)
    {
        try
        {
            Console.WriteLine($"Sending message to topic: {topic}");
            var producer = new ProducerBuilder<TKey, byte[]>(_config).Build();
            var result = await producer.ProduceAsync(topic, new Message<TKey, byte[]>
            {
                Key = key,
                Value = message,
                Headers = headers ?? new Headers()
            });

            Console.WriteLine($"Message sent to {result.TopicPartitionOffset}");
        }
        catch (ProduceException<TKey, byte[]> e)
        {
            Console.WriteLine($"Delivery failed: {e.Error.Reason}");
        }
    }

    public async Task SendMessageAwaitResponseAsyncRpc(string topic, byte[] message, Headers headers, string uuidProcess, Action<string> callBackFunction)
    {
        string masterRPC = M.Global.MasterRPC;
        Console.WriteLine("Master UUID: " + masterRPC + " Topic: " + topic + " Message: " + message + " Headers: " + headers);
        await SendMessageAsync(topic, message, headers);
        master.kfkConsumerListener.RegisterRPCFunction(masterRPC, uuidProcess, callBackFunction);
    }

    /// <summary>
    /// Send binary message without key
    /// </summary>
    public Task SendMessageAsync(string topic, byte[] message, Headers? headers = null)
    {
        return ProduceMessageAsync(topic, default(Null), message, headers);
    }

    /// <summary>
    /// Send binary message with key
    /// </summary>
    public Task SendMessageAsync(string topic, string key, byte[] message, Headers? headers = null)
    {
        return ProduceMessageAsync(topic, key, message, headers);
    }

    /// <summary>
    /// Send string message without key
    /// </summary>
    public Task SendMessageAsync(string topic, string message, Headers? headers = null)
    {
        return ProduceMessageAsync(topic, default(Null), System.Text.Encoding.UTF8.GetBytes(message), headers);
    }

    /// <summary>
    /// Send string message with key
    /// </summary>
    public Task SendMessageAsync(string topic, string key, string message, Headers? headers = null)
    {
        return ProduceMessageAsync(topic, key, System.Text.Encoding.UTF8.GetBytes(message), headers);
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