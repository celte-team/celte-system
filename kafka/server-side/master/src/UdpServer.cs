using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;


public sealed class UDPServer
{
    private static UDPServer? _instance;
    private static readonly object _lock = new object();
    private ServiceCompute _serviceCompute;
    public IOServiceReceive _ioServiceReceive;
    public IOServiceSend _ioServiceSend;
    public IPEndPoint? EndPoint;
    public UdpClient? UdpClient;

    public Dictionary<string, IHandler> Handlers;

    public Mutex mutex;

    private UDPServer()
    {
        _serviceCompute = new ServiceCompute();
        _ioServiceReceive = new IOServiceReceive();
        _ioServiceSend = new IOServiceSend();
        _serviceCompute.Start();
        _ioServiceReceive.Start();
        _ioServiceSend.Start();
        mutex = new Mutex();
        Handlers = new Dictionary<string, IHandler>();
    }

    ~UDPServer()
    {
        _serviceCompute.Stop();
        _ioServiceReceive.Stop();
        _ioServiceSend.Stop();
    }

    /// <summary>
    /// Add a new handler to the list of handlers
    /// </summary>
    /// <param name="ip"></param>
    /// <param name="port"></param>
    /// <param name="handlerType"></param>
    public void AddNewHandler(string ip, int port, HandlerType handlerType)
    {
        IHandler handler = HandlerFactory.CreateHandler(ip, port, handlerType);
        string key = ip + ":" + port;
        mutex.WaitOne();
        Handlers.Add(key, handler);
        mutex.ReleaseMutex();
    }


    /// <summary>
    /// Set the configuration of the server
    /// </summary>
    /// <param name="configuration"></param>
    public void SetConfig(Config configuration)
    {
        try {
            UdpClient = new UdpClient(configuration.Port);
            EndPoint = new IPEndPoint(IPAddress.Any, configuration.Port);
            // add the RL client to the list
            configureRL(configuration.RLIP, configuration.RLPort);
        } catch (Exception e) {
            Console.WriteLine("Error: " + e.Message);
            Environment.Exit(1);
        }
    }

    /// <summary>
    /// Configure the RL client
    /// </summary>
    public void configureRL(string ip, int port)
    {
        IHandler handler = HandlerFactory.CreateHandler(ip, port, HandlerType.RL);
        string key = ip + ":" + port;
        mutex.WaitOne();
        Handlers.Add(key, handler);
        mutex.ReleaseMutex();

        // add a message to the RL client buffer:
        string message = "Hello RL!";
        byte[] messageBytes = Encoding.ASCII.GetBytes(message);
        int opcode = 0;
        byte[] buffer = new byte[messageBytes.Length + 4];
        Buffer.BlockCopy(BitConverter.GetBytes(opcode), 0, buffer, 0, 4);
        // use the mutex of IOServiceSend
        _ioServiceSend.AddMessage((ip, port.ToString(), buffer));
    }

    public static UDPServer GetInstance()
    {
        if (_instance == null)
        {
            lock (_lock)
            {
                if (_instance == null)
                {
                    Console.WriteLine("Creating new instance of UDPServer");
                    _instance = new UDPServer();
                }
            }
        }
        return _instance;
    }
}
