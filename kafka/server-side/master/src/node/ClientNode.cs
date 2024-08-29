using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;

public static class ClientNode
{
    public static void AssignClientToNode((string?, string?, byte[]?) CurrentQueue)
    {
        UDPServer UdpServer = UDPServer.GetInstance();
        if (CurrentQueue.Item1 == null || CurrentQueue.Item2 == null || CurrentQueue.Item3 == null)
        {
            return;
        }

        var server = UDPServer.GetInstance();
        ClientHandler? clientHandler = server.Handlers[CurrentQueue.Item1 + ":" + CurrentQueue.Item2] as ClientHandler;
        if (clientHandler == null)
        {
            return;
        }


        // message format is <opcode 4 bytes><node ip (15 bytes)><node port (4 bytes)><client id (4 bytes)>
        NodeHandler node = clientHandler.AssignedNode;
        byte[]? message = new byte[27];
        byte[]? opcodeBytes = BitConverter.GetBytes((int)OpcodeEnum.ASSIGN_CLIENT_TO_NODE);
        byte[]? ipBytes = Encoding.ASCII.GetBytes(node._ip);
        // complete the ipBytes array to 15 bytes with null bytes
        Array.Resize(ref ipBytes, 15);
        byte[]? portBytes = BitConverter.GetBytes(node._port);
        byte[]? clientIdBytes = BitConverter.GetBytes(clientHandler.Id);

        if (ipBytes == null || portBytes == null || clientIdBytes == null)
        {
            return;
        }

        Array.Copy(opcodeBytes, 0, message, 0, 4);
        Array.Copy(ipBytes, 0, message, 4, 15);
        Array.Copy(portBytes, 0, message, 19, 4);
        Array.Copy(clientIdBytes, 0, message, 23, 4);

        (string?, string?, byte[]?) assignMessage = (CurrentQueue.Item1, CurrentQueue.Item2, message);
        UdpServer._ioServiceSend._queue.Enqueue(assignMessage);
    }
}
