using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Data.SqlTypes;

public static class ReplNode
{
    // public static void AssignClientToNode((string?, string?, byte[]?) CurrentQueue)
    // {
    //     Console.WriteLine("Assigning client to node: " + CurrentQueue.Item1 + ":" + CurrentQueue.Item2);
    //     UDPServer UdpServer = UDPServer.GetInstance();
    //     if (CurrentQueue.Item1 == null || CurrentQueue.Item2 == null || CurrentQueue.Item3 == null)
    //     {
    //         return;
    //     }
    //     foreach (KeyValuePair<string, IHandler> handler in UdpServer.Handlers)
    //     {
    //         if (handler.Value is AHandler aHandler && aHandler.handlerType == HandlerType.NODE)
    //         {
    //             string message = "Assigning client to node: " + CurrentQueue.Item1 + ":" + CurrentQueue.Item2;
    //             byte[] messageBytes = Encoding.ASCII.GetBytes(message);
    //             byte[] opcodeBytes = BitConverter.GetBytes((int)OpcodeEnum.ASSIGN_CLIENT_TO_NODE);
    //             byte[] buffer = new byte[opcodeBytes.Length + messageBytes.Length];
    //             Buffer.BlockCopy(opcodeBytes, 0, buffer, 0, opcodeBytes.Length);
    //             (string?, string?, byte[]?) assignMessage = (handler.Key.Split(":")[0], handler.Key.Split(":")[1], buffer);
    //             UdpServer._ioServiceSend._queue.Enqueue(assignMessage);
    //             Console.WriteLine("Client assigned to node: " + handler.Key.Split(":")[0] + ":" + handler.Key.Split(":")[1]);
    //             break;
    //         }
    //     }
    // }

    public static int SwitchCriteria(SNClientState state)
    {
        int nAvailableNodes = NodeHandler.Instances.Count;
        if (nAvailableNodes == 0)
        {
            return -1;
        }
        // we divide the y axis in nAvailableNodes parts, from -1000 to 1000
        float y = state.position[1];
        int node = (int)Math.Floor((y + 1000) / (2000 / nAvailableNodes));
        return node;
    }

    static List<(int playerId, SNClientState state)> UnpackData(byte[] data)
    {
        int offset = 0;

        // Read the number of players (int64)
        long nbPlayers = BitConverter.ToInt64(data, offset);
        offset += sizeof(long);

        List<(int playerId, SNClientState state)> players = new List<(int playerId, SNClientState state)>();

        // Size of SNClientState structure in bytes
        int stateSize = Marshal.SizeOf(typeof(SNClientState));

        for (long i = 0; i < nbPlayers; i++)
        {
            // Read player ID (int32)
            int playerId = BitConverter.ToInt32(data, offset);
            offset += sizeof(int);

            // Read SNClientState structure
            SNClientState state = ByteArrayToStructure<SNClientState>(data, offset);
            offset += stateSize;

            players.Add((playerId, state));
        }

        return players;
    }

    static T ByteArrayToStructure<T>(byte[] bytes, int offset) where T : struct
    {
        int size = Marshal.SizeOf(typeof(T));
        IntPtr ptr = Marshal.AllocHGlobal(size);

        try
        {
            Marshal.Copy(bytes, offset, ptr, size);
            return Marshal.PtrToStructure<T>(ptr);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    static ClientHandler? GetClientFromId(int id)
    {
        UDPServer UdpServer = UDPServer.GetInstance();
        foreach (KeyValuePair<string, IHandler> handler in UdpServer.Handlers)
        {
            if (handler.Value is AHandler aHandler && aHandler.handlerType == HandlerType.CLIENT)
            {
                ClientHandler clientHandler = (ClientHandler)aHandler;
                if (clientHandler.Id == id)
                {
                    return clientHandler;
                }
            }
        }
        return null;
    }

    public static void HandleGameState((string?, string?, byte[]?) CurrentQueue)
    {
        // ip, port, message
        Console.WriteLine("Handling game state.");
        UDPServer UdpServer = UDPServer.GetInstance();
        if (CurrentQueue.Item1 == null || CurrentQueue.Item2 == null || CurrentQueue.Item3 == null)
        {
            return;
        }
        List<(int playerId, SNClientState state)> players = UnpackData(CurrentQueue.Item3);

        // Process the list of players
        foreach (var (playerId, state) in players)
        {
            // Console.WriteLine($"Player ID: {playerId}, Position: {string.Join(", ", state.position)}, Velocity: {state.velocity}, Color: {string.Join(", ", state.color)}");
            ClientHandler? clientHandler = GetClientFromId(playerId);
            if (clientHandler != null)
            {
                clientHandler.state = state;
                NodeHandler? handler = NodeHandler.Instances[ReplNode.SwitchCriteria(state)] ?? throw new Exception("No available nodes.");
                if (handler != clientHandler.AssignedNode)
                {
                    var prevHandler = clientHandler.AssignedNode;
                    clientHandler.AssignedNode = handler;
                    startAuthorityTransfer(clientHandler, handler, prevHandler);
                }
            }
        }
    }

    static void startAuthorityTransfer(ClientHandler clientHandler, NodeHandler handler, NodeHandler prevHandler)
    {
        Console.WriteLine($"Starting authority transfer for client {clientHandler.Id} from {prevHandler._ip}:{prevHandler._port} to {handler._ip}:{handler._port}");
        byte[] dropAuthorityMessage = buildAuthorityTransferMessageNode(true, clientHandler);
        byte[] takeAuthorityMessage = buildAuthorityTransferMessageNode(false, clientHandler);
        byte[] notifyClientMessage = buildAuthorityTransferMessageClient(handler);

        (string?, string?, byte[]?) dropAuthority = (prevHandler._ip, prevHandler._port.ToString(), dropAuthorityMessage);
        (string?, string?, byte[]?) takeAuthority = (handler._ip, handler._port.ToString(), takeAuthorityMessage);
        (string?, string?, byte[]?) notifyClient = (clientHandler._ip, clientHandler._port.ToString(), notifyClientMessage);

        UDPServer UdpServer = UDPServer.GetInstance();
        UdpServer._ioServiceSend._queue.Enqueue(dropAuthority);
        UdpServer._ioServiceSend._queue.Enqueue(takeAuthority);
        UdpServer._ioServiceSend._queue.Enqueue(notifyClient);
    }

    static byte[] buildAuthorityTransferMessageNode(bool drops, ClientHandler clientHandler)
    {
        // message is <opcode (int 4 bytes)> <int clientId (4 bytes)><bool drops (4 bytes)><char[15] ip (15 bytes)><int port (4 bytes)>
        byte[] message = new byte[27];
        byte[] opcode = BitConverter.GetBytes((int)OpcodeEnum.AUTHORITY_TRANSFER);
        byte[] clientIdBytes = BitConverter.GetBytes(clientHandler.Id);
        byte[] dropsBytes = BitConverter.GetBytes(drops);
        byte[] ipBytes = Encoding.ASCII.GetBytes(clientHandler._ip);
        Array.Resize(ref ipBytes, 15);
        byte[] portBytes = BitConverter.GetBytes(clientHandler._port);

        Buffer.BlockCopy(opcode, 0, message, 0, opcode.Length);
        Buffer.BlockCopy(clientIdBytes, 0, message, opcode.Length, clientIdBytes.Length);
        Buffer.BlockCopy(dropsBytes, 0, message, opcode.Length + clientIdBytes.Length, dropsBytes.Length);
        Buffer.BlockCopy(ipBytes, 0, message, opcode.Length + clientIdBytes.Length + dropsBytes.Length, ipBytes.Length);
        Buffer.BlockCopy(portBytes, 0, message, opcode.Length + clientIdBytes.Length + dropsBytes.Length + ipBytes.Length, portBytes.Length);

        return message;
    }

    static byte[] buildAuthorityTransferMessageClient(NodeHandler node)
    {
        {
            // message is <int opcode (4 bytes)> <char[15] ip (15 bytes)><int port (4 bytes)>
            byte[] message = new byte[23];
            byte[] opcode = BitConverter.GetBytes((int)OpcodeEnum.WARN_CLIENT_AUTHORITY_TRANSFER);
            byte[] ipBytes = Encoding.ASCII.GetBytes(node._ip);
            Array.Resize(ref ipBytes, 15);
            byte[] portBytes = BitConverter.GetBytes(node._port);

            Buffer.BlockCopy(opcode, 0, message, 0, opcode.Length);
            Buffer.BlockCopy(ipBytes, 0, message, opcode.Length, ipBytes.Length);
            Buffer.BlockCopy(portBytes, 0, message, opcode.Length + ipBytes.Length, portBytes.Length);

            return message;
        }
    }
}
