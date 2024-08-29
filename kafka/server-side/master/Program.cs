using System;
using System.Threading;

static class Program
{
    static void Main(string[] args)
    {
        Config configuration = new Config();
        bool result = configuration.GetAllArgs(args);
        if (!result)
        {
            Console.WriteLine("Error reading config file");
            return;
        }
        UDPServer server = UDPServer.GetInstance();
        if (server == null)
        {
            Console.WriteLine("Error creating server instance");
            return;
        }
        server.SetConfig(configuration);
        Console.WriteLine("Press Enter to exit...");
        Console.ReadLine();
    }
}
