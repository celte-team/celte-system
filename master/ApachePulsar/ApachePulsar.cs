using System.Text;
using DotPulsar;
using DotPulsar.Extensions;
using DotPulsar.Abstractions;
using System.Buffers;

public class PulsarSingleton
{
    private static DotPulsar.PulsarClient? _client;
    private static readonly Dictionary<string, IProducer<ReadOnlySequence<byte>>> _producers = new();
    private static readonly Dictionary<string, IConsumer<ReadOnlySequence<byte>>> _consumers = new();

    public static void InitializeClient()
    {
        string pulsarBrokers = Environment.GetEnvironmentVariable("CELTE_PULSAR_HOST") ?? string.Empty;
        string pulsarPort = Environment.GetEnvironmentVariable("CELTE_PULSAR_PORT") ?? "6650";
        pulsarBrokers = "pulsar://" + pulsarBrokers + ":" + pulsarPort;
        if (string.IsNullOrEmpty(pulsarBrokers))
        {
            throw new ArgumentException("Pulsar brokers are not set.");
        }

        _client = (DotPulsar.PulsarClient)DotPulsar.PulsarClient.Builder()
                .ServiceUrl(new Uri(pulsarBrokers))
                .Build();
    }

    public static IProducer<ReadOnlySequence<byte>> GetProducer(string topic)
    {
        if (_client == null)
        {
            throw new InvalidOperationException("Pulsar client is not initialized.");
        }

        if (!_producers.ContainsKey(topic))
        {
            var producer = _client.NewProducer()
                .Topic(topic)
                .Create();
            Console.WriteLine($"Producer created for topic: {topic}");
            _producers[topic] = producer;
        }

        return _producers[topic];
    }

    public static IConsumer<ReadOnlySequence<byte>> GetConsumer(string topic, string subscriptionName, SubscriptionType subscriptionType = SubscriptionType.Shared)
    {
        if (_client == null)
        {
            throw new InvalidOperationException("Pulsar client is not initialized.");
        }

        var key = $"{topic}:{subscriptionName}";
        if (!_consumers.ContainsKey(key))
        {
            var consumer = _client.NewConsumer()
                .Topic(topic)
                .SubscriptionName(subscriptionName)
                .SubscriptionType(subscriptionType)
                .Create();
            _consumers[key] = consumer;
        }

        return _consumers[key];
    }

    public static async Task ProduceMessageAsync(string topic, string message)
    {
        var producer = GetProducer(topic);
        var messageBytes = Encoding.UTF8.GetBytes(message);
        var readOnlySequence = new ReadOnlySequence<byte>(messageBytes);
        await producer.Send(readOnlySequence);
    }

    public static async Task ConsumeMessagesAsync(string topic, string subscriptionName, Action<string> messageHandler, CancellationToken cancellationToken)
    {
        var consumer = GetConsumer(topic, subscriptionName);

        await foreach (var message in consumer.Messages(cancellationToken))
        {
            var data = message.Data.ToArray();
            var messageString = Encoding.UTF8.GetString(data);
            messageHandler(messageString);
            await consumer.Acknowledge(message, cancellationToken);
        }
    }

    public static async Task ShutdownAsync()
    {
        if (_client != null)
        {
            await _client.DisposeAsync();
            _client = null;
        }

        _producers.Clear();
        _consumers.Clear();
    }
}
