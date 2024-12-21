using System.Text;
using DotPulsar;
using DotPulsar.Extensions;
using DotPulsar.Abstractions;
using System.Buffers;

public class SubscribeOptions
{
    public string Topics { get; set; } = string.Empty;
    public string SubscriptionName { get; set; } = string.Empty;
    public Action<IConsumer<ReadOnlySequence<byte>>, string>? Handler { get; set; }
    public DotPulsar.Abstractions.IConsumer<ReadOnlySequence<byte>>? Consumer { get; set; } = null;
}

class PulsarConsumer
{
    // private readonly IPulsarClient _client;
    private readonly List<Task> _consumerTasks;
    private readonly CancellationTokenSource _cancellationTokenSource;


    public PulsarConsumer()
    {
        // Master master = Master.GetInstance();
        // // string pulsarBrokers = master._setupConfig.GetYamlObjectConfig()?["pulsar_brokers"]?.ToString() ?? string.Empty;
        // // get ip from env
        // string pulsarBrokers = Environment.GetEnvironmentVariable("PULSAR_BROKERS") ?? string.Empty;
        // if (string.IsNullOrEmpty(pulsarBrokers))
        // {
        //     throw new ArgumentException("Pulsar brokers are not set.");
        // }
        // _client = PulsarClient.Builder()
        //     .ServiceUrl(new Uri(pulsarBrokers))
        //     .Build();
        _consumerTasks = new List<Task>();
        _cancellationTokenSource = new CancellationTokenSource();
    }

    public void CreateConsumer(SubscribeOptions options)
    {
        if (options.Handler == null)
            throw new ArgumentException("Message handler is not set.");
        try
        {
            var consumer = Master.GetInstance().GetPulsarClient().NewConsumer()
                .Topic(options.Topics)
                .SubscriptionName(options.SubscriptionName)
                .Create();

            options.Consumer = consumer;
            // Start a task to process messages from this consumer
            var task = Task.Run(() => ConsumeMessagesAsync(options, _cancellationTokenSource.Token));
            _consumerTasks.Add(task);
            Redis.ActionLog actionLog = new Redis.ActionLog
            {
                ActionType = "CreateConsumer",
                Details = $"Created consumer for subscription {options.SubscriptionName}"
            };
            Redis.RedisClient.GetInstance().rLogger.LogActionAsync(actionLog).Wait();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error creating consumer: {ex.Message}");
        }
    }

    private async Task ConsumeMessagesAsync(SubscribeOptions options, CancellationToken cancellationToken)
    {
        if (options.Consumer == null)
        {
            Console.WriteLine("Consumer is not initialized.");
            return;
        }

        try
        {
            var consumer = options.Consumer;

            await foreach (var message in consumer.Messages(cancellationToken))
            {
                var data = message.Data.ToArray();
                Console.WriteLine($" -> Received message on {options.Topics}:\n {Encoding.UTF8.GetString(data)}");
                var messageString = Encoding.UTF8.GetString(data);
                options.Handler?.Invoke(consumer, messageString);

                // Acknowledge the message
                await consumer.Acknowledge(message, cancellationToken);
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            Console.WriteLine($"Error consuming messages for subscription {options.SubscriptionName}: {ex.Message}");
        }
    }

    ~PulsarConsumer()
    {
        ShutdownAsync().Wait();
    }

    public async Task ShutdownAsync()
    {
        Console.WriteLine("Shutting down consumers...");
        _cancellationTokenSource.Cancel();

        // Wait for all consumer tasks to complete
        await Task.WhenAll(_consumerTasks);

        Console.WriteLine("All consumers shut down.");
        await Master.GetInstance().GetPulsarClient().DisposeAsync();
    }
}