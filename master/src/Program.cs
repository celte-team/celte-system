// create a main method

using System;
using System.Threading;
using Confluent.Kafka;

class Program
{
    static void Main(string[] args)
    {
        Master master = Master.GetInstance();
        // listen for spacebar key press to exit
        master.StartMasterSystem();
        Console.WriteLine("Press the spacebar to exit...");

        master.KFKConsumer._uuidConsumerService.WelcomeNewEntry();
        while (Console.ReadKey().Key != ConsoleKey.Spacebar)
        {
            Thread.Sleep(1000);
            // WelcomeNewEntry
        }
        master._setupConfig.Shutdown();
    }
}