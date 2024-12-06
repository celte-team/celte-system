using System;
using System.Text;
using System.Threading.Tasks;
using Pulsar.Client.Api;

class PulsarProducer
{
    private readonly PulsarClient _client;
    private Master master = Master.GetInstance();

    public PulsarProducer()
    {
        try
        {
            var configObject = master._setupConfig?.GetYamlObjectConfig();
            if (configObject == null || configObject["pulsar_brokers"] == null)
            {
                throw new InvalidOperationException("Configuration object is null");
            }
            var pulsarConfig = configObject["pulsar_brokers"];
            string pulsar_brokers = pulsarConfig?.ToString() ?? throw new InvalidOperationException("Pulsar brokers configuration is null");
            Uri uri = new Uri(pulsar_brokers);
            _client = new PulsarClientBuilder()
                .ServiceUrl(pulsar_brokers)
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