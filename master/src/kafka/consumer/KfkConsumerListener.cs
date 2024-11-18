using Confluent.Kafka;
using System;
using System.Text;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Confluent.Kafka.Admin;
using MessagePack;

public class KfkConsumerListener : IDisposable
{
    private readonly IConsumer<string, byte[]> _consumer;
    private readonly ConcurrentQueue<(string Topic, byte[] Message, Headers)> _buffer;
    private readonly Dictionary<string, Action<byte[]>> _topicHandlers;
    private CancellationTokenSource _cancellationTokenSource;

    private readonly object _lock = new object();

    // AdminClientBuilder
    private readonly AdminClientConfig _adminClientConfig;

    public ConsumerConfig config;

    // map with uuid and function
    private Dictionary<string, Dictionary<string, Action<byte[]>>> _rpcFunctions;

    const string RedisChannelTopic = "kafka-topic";

    public KfkConsumerListener(string bootstrapServers, string groupId)
    {
        config = new ConsumerConfig
        {
            BootstrapServers = bootstrapServers,
            GroupId = groupId,
            AutoOffsetReset = AutoOffsetReset.Earliest
        };

        _consumer = new ConsumerBuilder<string, byte[]>(config).Build();
        _buffer = new ConcurrentQueue<(string, byte[], Headers)>();
        _topicHandlers = new Dictionary<string, Action<byte[]>>();
        _cancellationTokenSource = new CancellationTokenSource();
        // RPC function map
        _rpcFunctions = new Dictionary<string, Dictionary<string, Action<byte[]>>>();

        SubscribeToRedisChannelTopics();
        _adminClientConfig = new AdminClientConfig
        {
            BootstrapServers = bootstrapServers
        };
    }

    public void RegisterRPCFunction(string topic, string requestId, Action<byte[]> function)
    {
        if (!_rpcFunctions.ContainsKey(topic))
        {
            _rpcFunctions[topic] = new Dictionary<string, Action<byte[]>>();
        }
        _rpcFunctions[topic][requestId] = function;
        Console.WriteLine($"Registered RPC function for topic {topic}, request ID {requestId}");
    }

    public async void AddTopic(string topic, Action<byte[]>? handler, int partitions = 1)
    {
        using (var adminClient = new AdminClientBuilder(_adminClientConfig).Build())
        {
            try
            {
                var redisClient = Redis.RedisClient.GetInstance();

                redisClient.rLogger.LogActionAsync(new Redis.ActionLog
                {
                    ActionType = "KafkaConsumerListener",
                    Timestamp = DateTime.Now,
                    Details = "topic: " + topic
                }).GetAwaiter().GetResult();

                Console.WriteLine($"Checking if topic {topic} exists...");
                var metadata = adminClient.GetMetadata(TimeSpan.FromSeconds(10));
                Console.WriteLine($"Metadata: {metadata.Topics.Count}");
                if (!metadata.Topics.Any(t => t.Topic == topic))
                {
                    var topicSpecification = new TopicSpecification
                    {
                        Name = topic,
                        NumPartitions = partitions,
                        ReplicationFactor = 1
                    };
                    UpdateTopicFromRedisChannel(topic);
                    adminClient.CreateTopicsAsync(new List<TopicSpecification> { topicSpecification }).Wait();
                    Console.WriteLine($"Topic {topic} created successfully.\n\n");
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
                    if (handler != null)
                    {
                        _topicHandlers[topic] = handler;
                    }

                    var newSubscription = _consumer.Subscription.ToList();
                    newSubscription.Add(topic);
                    _consumer.Subscribe(newSubscription);
                }
            }
        }
    }

    /// <summary>
    /// The goal of this function is to update the topic list from a Redis channel, it will use the JSON format
    /// to send the topic name. then it their is a new topic it will add it to the topic list.
    /// </summary>
    public void SubscribeToRedisChannelTopics()
    {
        try
        {
            Console.WriteLine("Subscribing to Redis channel...");
            var redisClient = Redis.RedisClient.GetInstance();
            redisClient.Subscribe(RedisChannelTopic, (channel, message) =>
            {
                Console.WriteLine($"Received !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! message from Redis channel {channel}: {message}");
                string topic = message;
                if (_topicHandlers.ContainsKey(topic))
                {
                    Console.WriteLine($"Topic {topic} already exists.!!!!!!!!!!!!");
                    return;
                }
                Console.WriteLine($"Adding topic {topic} from Redis channel...");
                // AddTopic(topic, null);
        //                 const topicAction = [{
        //     topic: M.Global.MasterHelloSn,
        //     action: connectNode.connectNewNode
        // }, {
        //     topic: M.Global.MasterHelloClient,
        //     action: connectClient.connectNewClient
        // }]
                // AddTopic(topic, topicAction[0].action, 1);

            });
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error subscribing to Redis channel: {e.Message}");
        }
    }

    /// <summary>
    /// Add a topic to the topic JSON list from a Redis channel
    /// </summary>
    public void UpdateTopicFromRedisChannel(string topic) {
        try {
            var redisClient = Redis.RedisClient.GetInstance();
            Console.WriteLine($"Updating topic {topic} from Redis channel123123123...");
            redisClient.redisData.JSONPush(RedisChannelTopic, "$", topic);
        } catch (Exception e) {
            Console.WriteLine($"Error updating topic from Redis channel: {e.Message}");
        }
    }

    // Start consuming Kafka messages
    public void StartConsuming(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var consumeResult = _consumer.Consume(cancellationToken);
                if (consumeResult != null)
                {
                    _buffer.Enqueue((consumeResult.Topic, consumeResult.Message.Value, consumeResult.Message.Headers));
                    // _buffer.Enqueue((consumeResult.Topic, Encoding.ASCII.GetBytes(consumeResult.Message.Value), consumeResult.Message.Headers));
                    string messageString = System.Text.Encoding.UTF8.GetString(consumeResult.Message.Value);
                    Console.WriteLine($"Consumed event from topic {consumeResult.Topic}: value = {messageString}");
                }
                else
                {
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

    // Execute buffered messages
    public void StartExecuteBuffer(CancellationToken cancellationToken)
    {
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                if (_buffer.TryDequeue(out var item))
                {
                    // var (topic, message, headers) = item;
                    // give the good type to the variables
                    (string topic, byte[] message, Headers headers) = item;
                    string messageString = System.Text.Encoding.UTF8.GetString(message);
                    Console.WriteLine($"Processing message from topic {topic}: value = {messageString}");

                    // check if the 4 last characters are ".rpc"
                    if (topic.EndsWith(".rpc"))
                    {
                        Console.WriteLine($"Processing RPC message: topic = {topic}, message = {messageString}");
                        // byte[] messageBytes = Encoding.ASCII.GetBytes(message);
                        for (int i = 0; i < message.Length; i++)
                        {
                            Console.WriteLine($"Message byte {i}: {message[i]}");
                        }
                        Console.WriteLine($"Message in bytes: {string.Join(", ", message)}");


                        ProcessRPCMessage(topic, message, headers);
                    }
                    else if (_topicHandlers.ContainsKey(topic))
                    {
                        _topicHandlers[topic]?.Invoke(message); // TODO pass headers
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

    // Method to process RPC messages
    private void ProcessRPCMessage(string topic, byte[] message, Headers headers)
    {
        try
        {
            string answerId = System.Text.Encoding.UTF8.GetString(headers.GetLastBytes("answer"));
            _rpcFunctions[topic][answerId]?.Invoke(message);

        }
        catch (Exception e)
        {
            Console.WriteLine($"Error processing RPC message: {e.Message}");
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
