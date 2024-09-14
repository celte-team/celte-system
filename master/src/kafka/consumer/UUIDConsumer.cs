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

    /// <summary>
    /// Welcome new entry, in this function we will add all the logic to handle the new entry.
    /// </summary>
    /// <param name="message"></param>
    public void WelcomeNewEntry(string message)
    {
        Console.WriteLine("Welcome!!!!!!!!!!!!!! new entry.");
        Console.WriteLine($"Message: {message}");
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
