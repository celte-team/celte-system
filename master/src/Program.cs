using System;
using System.Threading;
using Confluent.Kafka;

class Program
{
    static void Main(string[] args)
    {
        Master master = Master.GetInstance();
        Console.WriteLine("Press Enter to exit...");
        Console.ReadLine();
        master.Dispose();
    }
}