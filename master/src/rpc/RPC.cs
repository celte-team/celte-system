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
        if (string.IsNullOrEmpty(id))
        {
            throw new ArgumentException("Scope ID cannot be null or empty.");
        }
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

    public static Scope Grape(string id)
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
    public static byte[] __str2bytes(string str)
    {
        return System.Text.Encoding.UTF8.GetBytes(str);
    }

    public static void __deserialize(string str, params object[] outObjects)
    {
        var bytes = __str2bytes(str);
        var deserializedObjects = MessagePackSerializer.Deserialize<object[]>(bytes);

        if (deserializedObjects.Length != outObjects.Length)
        {
            throw new ArgumentException("The number of deserialized objects does not match the number of output objects.");
        }

        for (int i = 0; i < outObjects.Length; i++)
        {
            outObjects[i] = deserializedObjects[i];
        }
    }

    public static void InvokeRemote(string rpcName, Scope scope, params object[] args)
    {
        if (string.IsNullOrEmpty(scope.Id))
        {
            throw new ArgumentException("Scope ID cannot be null or empty.");
        }
        byte[] data = MessagePackSerializer.Serialize(args);
        // var headers = new List<Header> { new Header("rpName", __str2bytes(rpcName)) };
        Headers headers;
        headers = new Headers { new Header("rpName", __str2bytes(rpcName)) };
        Master.GetInstance().kFKProducer.SendMessageAsync(scope.Id, data, headers);
    }

    /// <summary>
    /// this function is used to call a remote procedure on a specific node or client and wait for the response.
    /// </summary>
    /// <param name="rpcName"></param>
    /// <param name="scope"></param>
    /// <param name="args"></param>
    /// <param name="headers"></param>
    /// <returns></returns>
    public static async Task Call(string rpcName, Scope scope, Headers headers, Action<string> callBackFunction, params object[]? args)
    {
        byte[] data = MessagePackSerializer.Serialize(args);
        // faire une variable global de master uuid
        Console.WriteLine("Calling RPC: " + rpcName);
        await Master.GetInstance().kFKProducer.SendMessageAwaitResponseAsyncRpc(scope.Id, data, headers, callBackFunction);
    }

}