using System;
using System.Collections.Generic;
using System.Threading;
using System.Text;
using System.Runtime.CompilerServices;
public class ServiceCompute
{
    private Thread? _threadRead;
    private Thread? _threadProcess;
    public Queue<(string?, string?, byte[]?)> _queue;
    private readonly Mutex _mutex;
    private bool _running = false;

    public static readonly Dictionary<OpcodeEnum, Action<(string?, string?, byte[]?)>> Handler = new Dictionary<OpcodeEnum, Action<(string?, string?, byte[]?)>>
    {
        { OpcodeEnum.HELLO_SN, ServerNode.HandleNewNode },
        { OpcodeEnum.HELLO_CLIENT, ClientNode.AssignClientToNode },
        { OpcodeEnum.GAME_STATE, ReplNode.HandleGameState}
    };

    public ServiceCompute()
    {
        _queue = new Queue<(string?, string?, byte[]?)>();
        _mutex = new Mutex();
        _running = false;
    }

    ~ServiceCompute()
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
        _threadRead = new Thread(ComputeMessages) { IsBackground = true };
        _threadProcess = new Thread(ProcessMessages) { IsBackground = true };
        _threadRead.Start();
        _threadProcess.Start();
        Console.WriteLine("ServiceCompute started.");
    }

    public void Stop()
    {
        if (!_running)
        {
            return;
        }

        _running = false;
        if (_threadRead != null && _threadRead.IsAlive)
        {
            _threadRead.Join();
        }
        if (_threadProcess != null && _threadProcess.IsAlive)
        {
            _threadProcess.Join();
        }
        Console.WriteLine("ServiceCompute stopped.");
    }

    /// <summary>
    /// Compute messages from the clients and add them to the queue.
    /// </summary>
    private void ComputeMessages()
    {
        UDPServer UdpServer = UDPServer.GetInstance();

        while (_running)
        {
            try
            {
                _mutex.WaitOne();
                foreach (KeyValuePair<string, IHandler> handler in UdpServer.Handlers)
                {
                    if (handler.Value is AHandler aHandler)
                    {
                        if (aHandler.newMessage)
                        {
                            Console.WriteLine("New message found.");
                            aHandler.PopAllMessages(_queue);
                        }
                    }
                }
                _mutex.ReleaseMutex();
            }
            catch (Exception ex)
            {
                _mutex.ReleaseMutex();
                Console.WriteLine($"Error in ComputeMessages: {ex.Message}");
            }
        }
    }
    private void ProcessMessages()
    {
        while (_running)
        {
            try
            {
                if (_queue.Count > 0)
                {
                    _mutex.WaitOne();
                    var currentQueueItem = _queue.Dequeue();
                    _mutex.ReleaseMutex();

                    byte[]? message = currentQueueItem.Item3;

                    if (message == null)
                    {
                        Console.WriteLine("Received message is null.");
                        continue;
                    }

                    byte[] opcodeBytes = new byte[4];
                    Array.Copy(message, opcodeBytes, 4);
                    Console.WriteLine("Opcode bytes: " + BitConverter.ToString(opcodeBytes));
                    Console.WriteLine("Message bytes: " + BitConverter.ToString(message));
                    int opcodeInt = BitConverter.ToInt32(opcodeBytes, 0);

                    if (Enum.IsDefined(typeof(OpcodeEnum), opcodeInt))
                    {
                        OpcodeEnum opcode = (OpcodeEnum)opcodeInt;
                        message = message[4..];
                        Console.WriteLine($"Processing opcode: {opcode}");
                        Console.WriteLine("Message: " + Encoding.ASCII.GetString(message));
                        if (Handler.ContainsKey(opcode))
                        {
                            Handler[opcode](currentQueueItem);
                        }
                        else
                        {
                            Console.WriteLine("Unhandled opcode.");
                        }
                    }
                    else
                    {
                        Console.WriteLine("Invalid opcode received: " + opcodeInt);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in ProcessMessages: {ex.Message}");
            }
        }
    }
}
