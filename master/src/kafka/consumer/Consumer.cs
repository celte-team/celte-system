using Confluent.Kafka;
using System;
using System.Collections.Generic;

class KFKConsumer : IDisposable
{
    private ConsumerConfig _config;
    public UUIDConsumerService _uuidConsumerService;
    private Thread _uuidConsumerThread;
    public KFKConsumer()
    {
        try
        {
            _config = new ConsumerConfig
            {
                GroupId = "test-consumer-group",
                BootstrapServers = "localhost:80",
                AutoOffsetReset = AutoOffsetReset.Earliest
            };

            _uuidConsumerService = new UUIDConsumerService(_config);
            // _uuidConsumerService.WelcomeNewEntry();
            Console.WriteLine("UUIDConsumerService initialized.");
            // _uuidConsumerThread = new Thread(_uuidConsumerService.WelcomeNewEntry);
            // _uuidConsumerThread.Start();
            // _uuidConsumerService.WelcomeNewEntry();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error: {e.Message}");
        }
    }

    ~KFKConsumer()
    {
        Dispose();
    }

    public void Dispose()
    {
        _uuidConsumerService?.Dispose();
        // _uuidConsumerThread?.Join();
        Console.WriteLine("Kafka consumer disposed.");
    }
}