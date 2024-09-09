using Confluent.Kafka;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

public class KfkConsumerListener : IDisposable
{
    private readonly IConsumer<string, string> _consumer;
    private readonly ConcurrentQueue<(string Topic, string Message)> _buffer;
    private readonly Dictionary<string, Action<string>> _topicHandlers;
    private CancellationTokenSource _cancellationTokenSource;

    private readonly object _lock = new object();

    public KfkConsumerListener(string bootstrapServers, string groupId)
    {
        var config = new ConsumerConfig
        {
            BootstrapServers = bootstrapServers, // "localhost:80",
            GroupId = groupId, // "test-consumer-group",
            AutoOffsetReset = AutoOffsetReset.Earliest // "earliest"
        };

        _consumer = new ConsumerBuilder<string, string>(config).Build();
        _buffer = new ConcurrentQueue<(string, string)>();
        _topicHandlers = new Dictionary<string, Action<string>>();
        _cancellationTokenSource = new CancellationTokenSource();
    }

    public void AddTopic(string topic, Action<string> handler)
    {
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
                    _buffer.Enqueue((consumeResult.Topic, consumeResult.Message.Value));
                    Console.WriteLine($"Consumed event from topic {consumeResult.Topic}: value = {consumeResult.Message.Value}");
                } else {
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
