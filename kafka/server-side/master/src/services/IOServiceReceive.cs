using System;
using System.Net.Sockets;
using System.Reflection.Emit;
using System.Text;
using System.Threading;

public class IOServiceReceive
{
    private Thread? _thread;
    private bool _running;
    private readonly ManualResetEvent _stopEvent;

    public IOServiceReceive()
    {
        _stopEvent = new ManualResetEvent(false);
        _running = false;
    }

    ~IOServiceReceive()
    {
        Stop();
    }

    public void Start()
    {
        if (!_running)
        {
            _running = true;
            _stopEvent.Reset();
            _thread = new Thread(ReceiveMessages) { IsBackground = true };
            _thread.Start();
            Console.WriteLine("IOServiceReceive started.");
        }
    }

    public void Stop()
    {
        if (_running)
        {
            _running = false;
            _stopEvent.Set();
            if (_thread != null && _thread.IsAlive)
            {
                _thread.Join();
            }
            Console.WriteLine("IOServiceReceive stopped.");
        }
    }

    private void ReceiveMessages()
    {
        UDPServer udpServer = UDPServer.GetInstance();

        while (_running)
        {
            try
            {

                if (udpServer.UdpClient != null && udpServer.EndPoint != null && udpServer.UdpClient.Available > 0)
                {

                    byte[] receivedBytes = udpServer.UdpClient.Receive(ref udpServer.EndPoint);
                    
                    // ip of the sender
                    string ip = udpServer.EndPoint.Address.ToString();
                    int port = udpServer.EndPoint.Port;
                    // create new key.
                    string key = ip + ":" + port;
                    if (!udpServer.Handlers.ContainsKey(key))
                    {
                        byte[] buffer = new byte[4];
                        Buffer.BlockCopy(receivedBytes, 0, buffer, 0, 4);
                        int opcode = BitConverter.ToInt32(buffer, 0);
                        Console.WriteLine("Received opcode: " + opcode);
                        udpServer.mutex.WaitOne();
                        switch (opcode)
                        {
                            case > 199 and < 300:
                                udpServer.AddNewHandler(ip, port, HandlerType.NODE);
                                Console.WriteLine("Node connected: " + key);
                                break;
                            case > 299 and < 400:
                                udpServer.AddNewHandler(ip, port, HandlerType.CLIENT);
                                Console.WriteLine("Client connected: " + key);
                                break;
                            default:
                                Console.WriteLine("Unknown connection type: " + key);
                                break;
                        }
                        udpServer.mutex.ReleaseMutex();
                    }
                    Console.WriteLine("Received message from: " + key);
                    udpServer.Handlers[key].HandleMessage(receivedBytes);

                }

                // Wait for a short interval to avoid busy-waiting
                Thread.Sleep(10);

                // Check if the stop event is set
                if (_stopEvent.WaitOne(0))
                {
                    break;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in ReceiveMessages: {ex.Message}");
            }
        }
    }
}
