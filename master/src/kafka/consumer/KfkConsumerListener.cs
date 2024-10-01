using Confluent.Kafka;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Confluent.Kafka.Admin;

public class KfkConsumerListener : IDisposable
{
    private readonly IConsumer<string, string> _consumer;
    private readonly ConcurrentQueue<(string Topic, string Message)> _buffer;
    private readonly Dictionary<string, Action<string>> _topicHandlers;
    private CancellationTokenSource _cancellationTokenSource;

    private readonly object _lock = new object();

    // AdminClientBuilder
    private readonly AdminClientConfig _adminClientConfig;

    public ConsumerConfig config;

    // map with uuid and function
    private Dictionary<string, Action<string>> _rpcFunctions = new Dictionary<string, Action<string>>();

    public KfkConsumerListener(string bootstrapServers, string groupId)
    {
        config = new ConsumerConfig
        {
            BootstrapServers = bootstrapServers, // "localhost:80",
            GroupId = groupId,
            AutoOffsetReset = AutoOffsetReset.Earliest // "earliest"
        };

        _consumer = new ConsumerBuilder<string, string>(config).Build();
        _buffer = new ConcurrentQueue<(string, string)>();
        _topicHandlers = new Dictionary<string, Action<string>>();
        _cancellationTokenSource = new CancellationTokenSource();
        _adminClientConfig = new AdminClientConfig
        {
            BootstrapServers = bootstrapServers
        };
    }
    //rpc [uuid1: mapetiteFunction, uuid2: mapetiteFunction2]
    // open master.rpc change handler function
    public void AddTopic(string topic, Action<string> handler)
    {
        // use the _adminClientConfig
        using (var adminClient = new AdminClientBuilder(_adminClientConfig).Build())
        {
            try
            {
                // Check if the topic exists and create it if it doesn't
                var metadata = adminClient.GetMetadata(TimeSpan.FromSeconds(10));
                if (!metadata.Topics.Any(t => t.Topic == topic))
                {
                    var topicSpecification = new TopicSpecification
                    {
                        Name = topic,
                        NumPartitions = 1, // Specify the number of partitions
                        ReplicationFactor = 1 // Specify replication factor
                    };

                    adminClient.CreateTopicsAsync(new List<TopicSpecification> { topicSpecification }).Wait();
                    Console.WriteLine($"Topic {topic} created successfully.");
                }
                else
                {
                    Console.WriteLine($"Topic {topic} already exists.");
                }
            }
            catch (CreateTopicsException e)
            {
                Console.WriteLine($"An error occurred creating topic {topic}: {e.Results[0].Error.Reason}");
            }

            lock (_lock)
            {
                if (!_topicHandlers.ContainsKey(topic))
                {
                    // use an admin client to create the topic
                    _topicHandlers[topic] = handler;
                    var newSubscription = _consumer.Subscription.ToList();
                    newSubscription.Add(topic);
                    _consumer.Subscribe(newSubscription);
                    Console.WriteLine($"Registered handler for topic {topic}, newSubscription = {string.Join(",", newSubscription)}");
                }
            }
        }
    }

    public void StartConsuming(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                Console.WriteLine("StartConsuming");
                var consumeResult = _consumer.Consume(cancellationToken);
                if (consumeResult != null)
                {
                    // print the client id of the producer who produced the message
                    // Console.WriteLine($"Client Id: {consumeResult.Message.Headers.Last().GetValueBytes()}");
                    _buffer.Enqueue((consumeResult.Topic, consumeResult.Message.Value));
                    Console.WriteLine($"Consumed event from topic {consumeResult.Topic}: value = {consumeResult.Message.Value}");
                }
                else
                {
                    Console.WriteLine("No message consumed");
                    Thread.Sleep(100);
                }
            }
        }
        catch (OperationCanceledException)
        {
            Console.WriteLine("Operation cancelled");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error in StartConsuming: {ex.Message}");
        }
        finally
        {
            _consumer.Close();
        }
    }

    public void StartExecuteBuffer(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                if (_buffer.TryDequeue(out var item))
                {
                    var (topic, message) = item;
                    Console.WriteLine($"Processing message from topic {topic}: value = {message}");
                    if (_topicHandlers.ContainsKey(topic))
                    {
                        _topicHandlers[topic]?.Invoke(message);
                    }
                }
                else
                {
                    Thread.Sleep(100);
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error in StartExecuteBuffer: {ex.Message}");
        }
    }


    public void Dispose()
    {
        _consumer.Close();
        _consumer.Dispose();
        Console.WriteLine("Kafka consumer disposed.");
        _cancellationTokenSource.Cancel();
        _cancellationTokenSource.Dispose();
    }
}
