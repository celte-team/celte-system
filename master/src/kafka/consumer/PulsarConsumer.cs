using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Concurrent;
using System.Collections.Generic;
using DotPulsar;
using DotPulsar.Extensions;

class PulsarConsumer
{
    private Master master = Master.GetInstance();
    private DotPulsar.Abstractions.IPulsarClient? _client;
    private DotPulsar.Abstractions.IConsumer? _consumer;
    private readonly ConcurrentQueue<(string Topic, byte[] Message)> _buffer;
    private readonly Dictionary<string, Action<byte[]>> _listOfTopics;
    private CancellationTokenSource _cancellationTokenSource;
    private readonly object _lock = new object();

    public PulsarConsumer()
    {
        _buffer = new ConcurrentQueue<(string Topic, byte[] Message)>();
        _listOfTopics = new Dictionary<string, Action<byte[]>>();
        _cancellationTokenSource = new CancellationTokenSource();

        try
        {
            var configObject = master._setupConfig?.GetYamlObjectConfig();
            var pulsarConfig = (configObject?["pulsar_brokers"]) ?? throw new Exception("Pulsar brokers not found in configuration");
            string pulsarBrokers = pulsarConfig.ToString() ?? throw new Exception("Pulsar brokers not found in configuration");
            Uri pulsarUri = new(pulsarBrokers);

            _client = PulsarClient.Builder()
                .ServiceUrl(pulsarUri)
                .Build();
            Console.WriteLine("Pulsar client initialized successfully.");
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Pulsar consumer: {e.Message}");
        }
    }

    public void AddTopic(string topic, Action<byte[]>? handler)
    {
        lock (_lock)
        {
            if (!_listOfTopics.ContainsKey(topic))
            {
                _listOfTopics.Add(topic, handler ?? (msg => { }));
            }
            else
            {
                Console.WriteLine($"Handler already registered for topic: {topic}");
            }
        }
    }

    public async Task StartConsumingAsync(string topic, string subscriptionName)
    {
        try
        {
            // Initialize the consumer
            if (_client == null)
            {
                throw new InvalidOperationException("Pulsar client not initialized.");
            }

            // Create and subscribe to the topic using the correct DotPulsar API
            List<string>? topics = new List<string>();
            topics.Add(topic);
            _consumer = _client.NewConsumer()
                .Topic(topic)
                .SubscriptionName(subscriptionName)
                .Create();


            Console.WriteLine($"Consumer subscribed to topic: {topic}");

            // Start consuming messages
            while (!_cancellationTokenSource.Token.IsCancellationRequested)
            {
                var message = await _consumer.ReceiveAsync();
                if (message != null)
                {
                    Console.WriteLine($"Received message from topic {topic}: {Encoding.UTF8.GetString(message.Data)}");
                    _buffer.Enqueue((topic, message.Data));

                    // Acknowledge the message after processing
                    await _consumer.AcknowledgeAsync(message);
                }
            }
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error while consuming message: {e.Message}");
        }
    }

    public void StartExecuteBuffer()
    {
        try
        {
            while (!_cancellationTokenSource.Token.IsCancellationRequested)
            {
                if (_buffer.TryDequeue(out var item))
                {
                    var (topic, message) = item;

                    // Check if the topic has a handler and invoke it
                    if (_listOfTopics.ContainsKey(topic))
                    {
                        _listOfTopics[topic]?.Invoke(message);
                    }
                }
                else
                {
                    // Wait briefly if the buffer is empty
                    Task.Delay(100).Wait();
                }
            }
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error while executing buffer: {e.Message}");
        }
    }

    public void StopConsuming()
    {
        _cancellationTokenSource.Cancel();
        // _consumer?.Dispose();
        Console.WriteLine("Consuming stopped.");
    }
}