using System.Text.Json;
using Google.Protobuf;

/// <summary>
/// This class is a very lightweight rpc system to interface with
/// server node's rpcs using pulsar.
/// As the master is designed to be stateless, rpcs are limited to sending orders.
/// Responses are not handled. Use the HttpServer to receive orders.
/// </summary>
class RPC
{
    public static bool ConnectClientToNode(string nodeId, string spawnerId, string clientId, string sessionId)
    {
        string topic = $"non-persistent://public/{sessionId}/{nodeId}.rpc";
        Console.WriteLine($"Connecting client {clientId} to node {nodeId} on topic {topic}");
        try
        {
            Task.Run(async () =>
            {
                await Call(topic, "PeerService_call_AcceptNewClient", clientId, spawnerId);
            }).Wait();
            return true;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error connecting client to node: {ex.Message}");
            return false;
        }
    }

    public static async Task Call(string topic, string method, params object[] args)
    {
        var req = new Celte.Req.RPRequest();
        req.Name = method;
        req.RespondsTo = "";
        req.ResponseTopic = "";
        req.RpcId = new Random().Next().ToString();
        req.Args = JsonSerializer.Serialize(args);
        // req.Args = "[" + string.Join(",", args.Select(arg => JsonSerializer.Serialize(arg))) + "]";

        Google.Protobuf.JsonFormatter jsonFormatter = new JsonFormatter(new JsonFormatter.Settings(true));
        string jsonString = jsonFormatter.Format(req);

        await PulsarSingleton.ProduceMessageAsync(topic, jsonString);
    }
}