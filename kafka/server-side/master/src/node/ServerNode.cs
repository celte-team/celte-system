using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;


public static class ServerNode
{
    public static void HandleNewNode((string?, string?, byte[]?) CurrentQueue)
    {
        UDPServer UdpServer = UDPServer.GetInstance();
        if (CurrentQueue.Item1 == null || CurrentQueue.Item2 == null || CurrentQueue.Item3 == null)
        {
            return;
        }

        foreach (KeyValuePair<string, IHandler> handler in UdpServer.Handlers)
        {
            if (handler.Value is AHandler aHandler && aHandler.handlerType == HandlerType.RL)
            {
                var rl = handler.Value as ReplicationLayerHandler;
                if (rl == null)
                {
                    Console.WriteLine("Error: rl is null.");
                    return;
                }
                // message <opcode 4 bytes><rl ip 15 bytes><rl port 4 bytes>
                byte[]? message = new byte[23];
                byte[]? opcodeBytes = BitConverter.GetBytes((int)OpcodeEnum.NEW_NODE);
                byte[]? ipBytes = Encoding.ASCII.GetBytes(rl._ip);
                // complete the ipBytes array to 15 bytes with null bytes
                Array.Resize(ref ipBytes, 15);
                byte[]? portBytes = BitConverter.GetBytes(rl._port);

                if (ipBytes == null || portBytes == null)
                {
                    return;
                }

                Array.Copy(opcodeBytes, 0, message, 0, 4);
                Array.Copy(ipBytes, 0, message, 4, 15);
                Array.Copy(portBytes, 0, message, 19, 4);

                (string?, string?, byte[]?) welcomeMessage = (CurrentQueue.Item1, CurrentQueue.Item2, message);
                UdpServer._ioServiceSend._queue.Enqueue(welcomeMessage);
                return;
            }
        }
    }
}
