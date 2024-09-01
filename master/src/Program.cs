// create a main method

using System;
using System.Threading;
using Confluent.Kafka;

class Program
{
    static void Main(string[] args)
    {
        Master master = Master.GetInstance();
        master._setupConfig = new SetupConfig(args);
        master._setupConfig.SettingUpMaster();
        // setup Kafka manager
        master.setupKafkaManager();
        // listen for spacebar key press to exit
        Console.WriteLine("Press the spacebar to exit...");
        while (Console.ReadKey().Key != ConsoleKey.Spacebar)
        {
            Thread.Sleep(1000);
        }
        master._setupConfig.Shutdown();
    }
}