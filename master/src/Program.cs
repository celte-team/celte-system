
using System;
using System.Threading;
using Confluent.Kafka;

class Program
{
    static void Main(string[] args)
    {
        // var root = Directory.GetCurrentDirectory();
        // var dotenv = Path.Combine(root, ".env");
        // DotEnv.Load(dotenv);
        // Console.WriteLine(Environment.GetEnvironmentVariable("REDIS_PASSWORD"));
        // string redisPassword = Environment.GetEnvironmentVariable("REDIS_PASSWORD");
        // RedisSystem redisSystem = new RedisSystem(redisPassword);
        Redis.RedisClient redisClient = Redis.RedisClient.GetInstance("localhost:6379");
        Master master = Master.GetInstance();
        Console.CancelKeyPress += (sender, e) =>
        {
            e.Cancel = true;
            Console.WriteLine("Ctrl+C pressed, exiting...");
            master.Dispose();
            Environment.Exit(0);
        };

        Console.WriteLine("Press Ctrl+C to exit...");
        while (true)
        {
            Thread.Sleep(100);
        }
        master.Dispose();
    }
}