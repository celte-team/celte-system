
using System;
using System.Threading;

class Program
{
    static void Main(string[] args)
    {
        try {
        Master master = Master.GetInstance();
            Console.CancelKeyPress += (sender, e) =>
            {
                e.Cancel = true;
                Console.WriteLine("Ctrl+C pressed, exiting...");
                master.Dispose();
                Environment.Exit(0);
            };
        } catch (Exception e) {
            Console.WriteLine($"Error initializing Master: {e.Message}");
            Environment.Exit(1);
        }

        Console.WriteLine("\nPress Ctrl+C to exit...\n");
        while (true)
        {
            Thread.Sleep(100);
        }
    }
}