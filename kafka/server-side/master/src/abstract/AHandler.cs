using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using CircularBuffer;

public abstract class AHandler : IHandler
{
    public CircularBuffer<byte> _circularBuffer;
    public string _ip;
    public int _port;
    public Mutex mutex;
    public bool newMessage;
    public string serverIp;
    public int serverPort;

    public HandlerType handlerType;

    public AHandler(string ip, int port)
    {
        _circularBuffer = new CircularBuffer<byte>(1024);
        _ip = ip;
        _port = port;
        mutex = new Mutex();
        newMessage = false;
        serverIp = "";
        serverPort = 0;
    }

    public void HandleMessage(byte[] message)
    {
        mutex.WaitOne();
        foreach (var b in message)
        {
            _circularBuffer.PushBack(b);
        }
        newMessage = true;
        mutex.ReleaseMutex();
    }

    public (string?, string?, byte[]?) TryPopMessage()
    {
        bool isBackSlashR = false;
        mutex.WaitOne();
        if (_circularBuffer.Size == 0)
        {
            mutex.ReleaseMutex();
            return (null, null, null);
        }

        List<byte> messageBytes = new List<byte>();
        while (_circularBuffer.Size > 0)
        {
            var byteValue = _circularBuffer.Front();
            _circularBuffer.PopFront();

            if (byteValue == 13) // \r
            {
                isBackSlashR = true;
            }
            else if (byteValue == 10 && isBackSlashR) // \n
            {
                break;
            }
            else
            {
                isBackSlashR = false;
            }

            messageBytes.Add(byteValue);
        }

        byte[] message = messageBytes.ToArray();
        var parts = (_ip, _port.ToString(), message);
        mutex.ReleaseMutex();
        return parts;
    }

    public void PopAllMessages(Queue<(string?, string?, byte[]?)> queue)
    {
        while (true)
        {
            var messageParts = TryPopMessage();
            if (messageParts.Item1 == null && messageParts.Item2 == null && messageParts.Item3 == null)
            {
                break;
            }
            queue.Enqueue(messageParts);
        }
        newMessage = false;
    }
}
