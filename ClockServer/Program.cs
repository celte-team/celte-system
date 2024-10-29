using System;
using System.Threading;
using System.Threading.Tasks;
using Confluent.Kafka;
using MessagePack;

namespace GlobalClockService
{
    class Program
    {
        static async Task Main(string[] args)
        {
            var config = new ProducerConfig
            {
                BootstrapServers = System.Environment.GetEnvironmentVariable("CELTE_HOST_CLUSTER") ?? "localhost:9092"
            };

            int deltaMs = int.TryParse(System.Environment.GetEnvironmentVariable("CELTE_CLOCK_DELTA_MS"), out var result) ? result : 1000 / 10; // defaults at 10 fps

            using var producer = new ProducerBuilder<string, string>(config).Build();
            var topic = "global.clock";

            var cts = new CancellationTokenSource();
            Console.CancelKeyPress += (_, e) =>
            {
                e.Cancel = true;
                cts.Cancel();
            };

            Console.WriteLine("Press Ctrl+C to stop the global clock...");

            try
            {
                await StartClockTicks(producer, topic, deltaMs, cts.Token);
            }
            catch (OperationCanceledException)
            {
                Console.WriteLine("Global clock stopped.");
            }
        }

        static async Task StartClockTicks(IProducer<string, string> producer, string topic, int deltaMs, CancellationToken cancellationToken)
        {
            long tickId = 0;

            while (!cancellationToken.IsCancellationRequested)
            {
                var timestamp = DateTime.UtcNow;
                var headers = new Headers
                {
                    new Header("deltaMs", MessagePackSerializer.Serialize(deltaMs))
                };

                var message = new Message<string, string>
                {
                    Key = tickId.ToString(),
                    Value = Convert.ToBase64String(MessagePackSerializer.Serialize(tickId)),
                    Headers = headers
                };

                try
                {
                    var deliveryResult = await producer.ProduceAsync(topic, message, cancellationToken);
                }
                catch (ProduceException<string, string> e)
                {
                    Console.WriteLine($"Failed to deliver message: {e.Message} [{e.Error.Code}]");
                }

                tickId++;
                await Task.Delay(deltaMs, cancellationToken); // Interval between ticks
            }
        }
    }
}