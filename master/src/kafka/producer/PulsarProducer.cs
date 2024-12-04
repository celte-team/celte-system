
using System.Text;
using Pulsar.Client.Api;

class PulsarProducer
{
    private readonly PulsarClient _client;
    private Master master = Master.GetInstance();

    RPC rpc = new RPC();

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
            Console.WriteLine($"topic is: {topic}");
            Console.WriteLine($"Producing message!!!!!!!!!!: {message}");
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
    ~PulsarProducer()
    {
        _client.CloseAsync().Wait();
    }
}