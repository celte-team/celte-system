
using System;
using System.Threading;

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

        Console.WriteLine("\nPress Ctrl+C to exit...\n");
        while (true)
        {
            Thread.Sleep(100);
        }
    }
}