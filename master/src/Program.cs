
using System;
using System.Threading;

class Program
{
    static void Main(string[] args)
    {
        try
        {
            Master master = Master.GetInstance();
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                Console.WriteLine("Ctrl+C pressed, exiting...");
                Redis.RedisClient redis = Redis.RedisClient.GetInstance();
                redis.redisData.JSONRemove("nodes");
                master.Dispose();
                Environment.Exit(0);
            };
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Master: {e.Message}");
            Environment.Exit(1);
        }

        Console.WriteLine("\nPress Space to exit...\n");

        while (true)
        {
            if (Console.KeyAvailable)
            {
                var key = Console.ReadKey(intercept: true).Key;
                if (key == ConsoleKey.Spacebar)
                {
                    Console.WriteLine("Space key pressed, exiting...");
                    Master.GetInstance().Dispose();
                    // delete node from redis
                    Redis.RedisClient redis = Redis.RedisClient.GetInstance();
                    redis.redisData.JSONRemove("nodes");
                    // redis.redisData.JSONRemove("action_logs_master");
                    // redis.redisData.JSONRemove("logs");
                    // redis.redisData.JSONRemove("clients_try_to_connect");
                    Environment.Exit(0);
                }
            }
            Thread.Sleep(100);
        }
    }
}