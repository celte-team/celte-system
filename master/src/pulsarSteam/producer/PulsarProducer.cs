using System.Text;
using DotPulsar.Extensions;
class PulsarProducer
{
    private Master master = Master.GetInstance();

    public PulsarProducer()
    {
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