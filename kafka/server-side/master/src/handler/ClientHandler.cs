using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;

using System.Runtime.InteropServices;


[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct SNClientState
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
    public float[] position;

    public float velocity;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
    public float[] color;
}


public class ClientHandler : AHandler
{
    public SNClientState state;
    public int Id;
    public static int UuidGenerator = 1;
    public NodeHandler AssignedNode;

    public ClientHandler(string ip, int port) : base(ip, port)
    {
        _ip = ip;
        _port = port;
        handlerType = HandlerType.CLIENT;
        state = new SNClientState();

        NodeHandler? handler = NodeHandler.Instances[ReplNode.SwitchCriteria(state)] ?? throw new Exception("No available nodes.");
        Console.WriteLine("Client assigned to node: " + handler._ip + ":" + handler._port);
        AssignedNode = handler;
        assignClientId();
    }

    private void assignClientId()
    {
        Id = UuidGenerator;
        UuidGenerator++;
    }
}
