using Confluent.Kafka;
using System;

class UUIDConsumerService : IDisposable
{
    private Master _master;
    public UUIDConsumerService()
    {
        // Initialize Kafka consumer configuration
        _master = Master.GetInstance();
    }

    public void Dispose()
    {
        Console.WriteLine("Kafka consumer disposed.");
    }

    ~UUIDConsumerService()
    {
        Dispose();
    }
}
