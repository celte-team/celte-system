using System;
using System.Threading;
using Confluent.Kafka;

class Program
{
    static void Main(string[] args)
    {
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