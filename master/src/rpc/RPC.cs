using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using MessagePack;
using Confluent.Kafka;

public class Scope
{
    private string id;

    private Scope(string id)
    {
        this.id = id + ".rpc";
    }

    public string Id => id;

    public static Scope Peer(string id)
    {
        return new Scope(id);
    }

    public static Scope Chunk(string id)
    {
        return new Scope(id);
    }

    public static Scope Grappe(string id)
    {
        return new Scope(id);
    }

    public static Scope Global()
    {
        return new Scope(null);
    }
}


/// <summary>
/// Remote Procedure Call (RPC) class to invoke remote methods on other nodes or clients.
/// This implementation does not support calling registering rpcs that can be called remotely to execute
/// in the master server.
/// </summary>
class RPC
{
    private static byte[] __str2bytes(string str)
    {
        return System.Text.Encoding.UTF8.GetBytes(str);
    }

    public static void InvokeRemote(string rpcName, Scope scope, params object[] args)
    {
        byte[] data = MessagePackSerializer.Serialize(args);
        // var headers = new List<Header> { new Header("rpName", __str2bytes(rpcName)) };
        Headers headers;
        headers = new Headers { new Header("rpName", __str2bytes(rpcName)) };
        Master.GetInstance().kFKProducer.SendMessageAsync(scope.Id, data, headers);
    }

    // Add more Register methods for different numbers of arguments as needed
}