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
            var producer = Master.GetInstance().GetPulsarClient().NewProducer()
                .Topic(topic)
                .Create();
            Redis.ActionLog actionLog = new Redis.ActionLog
            {
                ActionType = "ProduceMessage",
                Details = $"Produced message to topic {topic}"
            };
            Redis.RedisClient.GetInstance().rLogger.LogActionAsync(actionLog).Wait();
            // string msg = message.ToString();
            // Console.WriteLine($"Producing message!!!!!!!!!!!!: {msg}\n\n");
            // await producer.Send(Encoding.UTF8.GetBytes(msg));
            Google.Protobuf.JsonFormatter jsonFormatter = new JsonFormatter(new JsonFormatter.Settings(true));
            string jsonString = jsonFormatter.Format(message);
            Console.WriteLine($"Producing message!!!!!!!!!!!!: {jsonString}\n\n");
            await producer.Send(Encoding.UTF8.GetBytes(jsonString));
            // await producer.Send(jsonString);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error producing message: {e.Message}");
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