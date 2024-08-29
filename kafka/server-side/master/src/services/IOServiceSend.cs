// thread
// container: circular buffer or queue
// mutex for container
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;

public class IOServiceSend
{
    public Thread? _thread;
    public Queue<(string ?, string ?, byte[] ?)> _queue;
    private Mutex _mutex;
    private bool _running;

    public IOServiceSend()
    {
        _mutex = new Mutex();
        _queue = new Queue<(string ?, string ?, byte[] ?)>();
        _running = false;
        _thread = null;
    }

    ~IOServiceSend()
    {
        Stop();
    }

    public void Start()
    {
        if (_running)
        {
            return;
        }
        _running = true;
        _thread = new Thread(SendMessages) { IsBackground = true };
        _thread.Start();
    }

    public void Stop()
    {
        if (_running)
        {
            _running = false;
            if (_thread != null && _thread.IsAlive)
            {
                _thread.Join();
            }
        }
        Console.WriteLine("IOServiceSend stopped.");
    }


    /// <summary>
    /// Add a message to the queue
    /// </summary>
    public void AddMessage((string ?, string ?, byte[] ?) message)
    {
        _mutex.WaitOne();
        _queue.Enqueue(message);
        _mutex.ReleaseMutex();
    }

    /// <summary>
    /// Used to send messages to the clients
    /// </summary>
    public void SendMessages()
    {
        while (_running)
        {
            try
            {
                if (_queue.Count == 0)
                {
                    Thread.Sleep(100); // Avoid busy waiting
                    continue;
                }
                _mutex.WaitOne();
                Console.WriteLine("Sending message");
                var (ip, port, messageBytes) = _queue.Dequeue();
                _mutex.ReleaseMutex();

                if (port == null || ip == null || messageBytes == null)
                {
                    continue;
                }
                int portNumber = int.Parse(port);

                UDPServer UdpServer = UDPServer.GetInstance();
                if (UdpServer.UdpClient == null)
                {
                    Console.WriteLine("UdpClient is null");
                    continue;
                }
                UdpServer.UdpClient.Send(messageBytes, messageBytes.Length, ip, portNumber);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error: {ex.Message}");
            }
        }
    }
}
