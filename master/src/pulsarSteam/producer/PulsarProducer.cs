using System;
using System.Text;
using System.Threading.Tasks;
using Pulsar.Client.Api;

class PulsarProducer
{
    private readonly PulsarClient? _client;
    private Master master = Master.GetInstance();

    public PulsarProducer()
    {
        try
        {
            string pulsarBrokers = Environment.GetEnvironmentVariable("PULSAR_BROKERS") ?? string.Empty;
            if (string.IsNullOrEmpty(pulsarBrokers))
            {
                throw new ArgumentException("Pulsar brokers are not set.");
            }
            Uri uri = new Uri(pulsarBrokers);
            _client = new PulsarClientBuilder()
                .ServiceUrl(pulsarBrokers)
                .BuildAsync().Result;
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Pulsar producer: {e.Message}");
        }
    }

    public async Task ProduceMessageAsync(string topic, string message)
    {
        try
        {
            var producer = await _client.NewProducer()
                .Topic(topic)
                .CreateAsync();
            Redis.ActionLog actionLog = new Redis.ActionLog
            {
                ActionType = "ProduceMessage",
                Details = $"Produced message to topic {topic}"
            };
            Redis.RedisClient.GetInstance().rLogger.LogActionAsync(actionLog).Wait();
            await producer.SendAsync(Encoding.UTF8.GetBytes(message));
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error producing message: {e.Message}");
        }
    }

    public async Task OpenTopic(string topic)
    {
        try
        {
            var producer = await _client.NewProducer()
                .Topic(topic)
                .CreateAsync();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error opening topic: {e.Message}");
        }
    }

    public async Task SendMessageAwaitResponseAsyncRpc(string topic, byte[] message, string uuidProcess, Action<byte[]> callBackFunction)
    {
        // Here will be implemented the logic to send a message and wait for a response to trigger a callback function
    }

    public void Dispose()
    {
        // _client.Close();
        // _client.Dispose();
    }

    ~PulsarProducer()
    {
        Dispose();
    }
}