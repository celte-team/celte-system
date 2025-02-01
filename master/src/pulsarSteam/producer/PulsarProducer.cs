using System.Text;
using DotPulsar.Extensions;
using Google.Protobuf;
class PulsarProducer
{
    private Master master = Master.GetInstance();

    public PulsarProducer()
    {
    }


    // ProduceMessageAsync protobuf

public async Task ProduceMessageAsync(string topic, Celte.Req.RPRequest message)
{
    try
    {
        var pulsarClient = Master.GetInstance().GetPulsarClient();
        if (pulsarClient == null)
        {
            Console.WriteLine("Error: Pulsar client is not initialized.");
            return;
        }

        var producer = pulsarClient.NewProducer()
            .Topic(topic)
            .Create();

        Google.Protobuf.JsonFormatter jsonFormatter = new JsonFormatter(new JsonFormatter.Settings(true));
        string jsonString = jsonFormatter.Format(message);

        Console.WriteLine($"[DEBUG] Producing message: {jsonString}\n");

        if (string.IsNullOrWhiteSpace(jsonString))
        {
            Console.WriteLine("Error: JSON string is empty. Aborting.");
            return;
        }

        await producer.Send(Encoding.UTF8.GetBytes(jsonString));
        Console.WriteLine($"Successfully produced message to {topic}");

        var actionLog = new Redis.ActionLog
        {
            ActionType = "ProduceMessage",
            Details = $"Produced message to topic {topic}"
        };
        await Redis.RedisClient.GetInstance().rLogger.LogActionAsync(actionLog);
    }
    catch (Exception e)
    {
        Console.WriteLine($"Error producing message: {e}");
    }
}

    public async Task ProduceMessageAsync(string topic, string message)
    {
        try
        {
            var producer = Master.GetInstance().GetPulsarClient().NewProducer()
                .Topic(topic)
                .Create();
            Redis.ActionLog actionLog = new Redis.ActionLog
            {
                ActionType = "ProduceMessage",
                Details = $"Produced message to topic {topic}"
            };
            Redis.RedisClient.GetInstance().rLogger.LogActionAsync(actionLog).Wait();
            await producer.Send(Encoding.UTF8.GetBytes(message));
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
            var producer = (Master.GetInstance().GetPulsarClient()).NewProducer()
                .Topic(topic)
                .Create();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error opening topic: {e.Message}");
        }
    }

    ~PulsarProducer()
    {
    }
}