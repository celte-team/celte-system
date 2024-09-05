using Confluent.Kafka;
using System;

class UUIDConsumerService : IDisposable
{
    private readonly ConsumerConfig _config;

    public UUIDConsumerService(ConsumerConfig config)
    {
        // Initialize Kafka consumer configuration
        _config = config;
    }

    public void WelcomeNewEntry()
    {
        Console.WriteLine("Welcome!!!!!!!!!!!!!! new entry.");

        using (var consumer = new ConsumerBuilder<string, string>(_config).Build())
        {
            Console.WriteLine("Waiting for new entry...");
            consumer.Subscribe("NewEntry");

            try
            {
                while (true)
                {
                    var cr = consumer.Consume();
                    Console.WriteLine($"Consumed event from topic NewEntry: value = {cr.Message.Value}");
                }
            }
            catch (OperationCanceledException)
            {
                // Handle cancellation, such as Ctrl-C
            }
            finally
            {
                consumer.Close(); // Ensure the consumer is closed properly
            }
        }
    }

    public void Dispose()
    {
        Console.WriteLine("Kafka consumer disposed.");
        GC.SuppressFinalize(this);
    }

    ~UUIDConsumerService()
    {
        Dispose();
    }
}
