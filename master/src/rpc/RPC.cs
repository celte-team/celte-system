using System.Text.Json;
using Celte.Req;
using Confluent.Kafka;
using Microsoft.Extensions.Options;
// public struct RPRequest
// {
//     public string name { get; set; }
//     public string respondsTo { get; set; }
//     public string responseTopic { get; set; }
//     public string rpcId { get; set; }
//     public JsonElement args { get; set; }

//     public string ToJson()
//     {
//         return JsonSerializer.Serialize(this);
//     }

//     public static RPRequest FromJson(string jsonString)
//     {
//         return JsonSerializer.Deserialize<RPRequest>(jsonString);
//     }
// }

/// <summary>
///
/// This class is a simple RPC system that allows you to register methods to be called remotely.
/// Example usage:
/// // creating a method that can be called on this peer by a remote process
/// RPC.Register("myMethod", (args) => {
///    Console.WriteLine("Received RPC call with args: " + args);
///    return new JsonElement("Hello from RPC!");
///    });
///
/// RPC.InitConsumer();
///
/// // calling the method on a remote peer
///
/// // do this registeration only once at the beginning of the program
/// RPC.RegisterResponseHandler("myRemoteMethod", (args) => {
///   Console.WriteLine("Received response from RPC call: " + args);
///   });
///
/// RPC.Call("myRemotePeerTopic", "myRemoteMethod", new JsonElement("Hello from RPC!"));
///
/// </summary>
public class RPC
{
    private static Dictionary<string, Func<JsonElement, JsonElement?>> _rpcHandlers = new Dictionary<string, Func<JsonElement, JsonElement?>>();
    private static Dictionary<string, Action<Celte.Req.RPRequest>> _rpcResponseHandlers = new Dictionary<string, Action<Celte.Req.RPRequest>>();

    public static void Register(string methodName, Func<JsonElement, JsonElement?> handler)
    {
        _rpcHandlers[methodName] = handler;
    }

    public static void RegisterResponseHandler(string methodName, Action<Celte.Req.RPRequest> handler)
    {
        _rpcResponseHandlers[methodName] = handler;
        Console.WriteLine($"<RegisterResponseHandler> Registered response handler for {methodName}");
    }

    public void InitConsumer()
    {
        Master.GetInstance().pulsarConsumer.CreateConsumer(new SubscribeOptions
        {
            Topics = "persistent://public/default/master.rpc",
            SubscriptionName = "master.rpc",
            Handler = (consumer, message) => __handleRPC(message)
        });
    }

    private static async void __handleRPC(string message)
    {
        try
        {
            // Deserialize the incoming JSON message into a Protobuf object (RPRequest)
            RPRequest request = RPRequest.Parser.ParseJson(message);
            if (request == null)
            {
                throw new InvalidOperationException("Failed to parse RPC request.");
            }

            Console.WriteLine($"<RPC> Received RPC call {request.Name} with args {request.Args} and respondsTo {request.RespondsTo}");

            // Handle response if it's a response and not a new RPC call
            if (request.RespondsTo != "")
            { // this is a response, not a call
                if (_rpcResponseHandlers.TryGetValue(request.Name, out Action<Celte.Req.RPRequest> v))
                {
                    v(request);
                    Console.WriteLine($"<RPC> __handleRPC Handled response NO RESPONSE NEEDED");
                }
                return;
            }

            if (!string.IsNullOrEmpty(request.RespondsTo)) // This is a response, not a call
            {
                if (_rpcResponseHandlers.TryGetValue(request.Name, out Action<Celte.Req.RPRequest> responseHandler))
                {
                    responseHandler(request);
                    Console.WriteLine("<RPC> Handled response");
                }
                return;
            }

            // Handle normal RPC call
            Console.WriteLine($"<RPC> Received RPC call::::: {request.Name} with args {request.Args} Sending response to {request.ResponseTopic} \n\n");
            // If there's a response topic, send back the response
            if (!string.IsNullOrEmpty(request.ResponseTopic))
            {
                // Create a new response request
                var responseRequest = new RPRequest
                {
                    Name = request.Name,
                    RespondsTo = request.RespondsTo,
                    ResponseTopic = "", // we don't want a response to this response
                    RpcId = request.RpcId,
                    Args = request.Args
                };
                // Send the response to the corresponding topic
                await Master.GetInstance().pulsarProducer.ProduceMessageAsync(request.ResponseTopic, responseRequest);
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"<RPC> Error while handling RPC: {ex.Message} \n\n");
        }
    }

    /// <summary>
    /// calls a method on the given remote topic, and *does not* wait for a response
    /// The request object must be serializable to JSON.
    /// </summary>
    ///

    public async static void Call(string topic, string methodName, Celte.Req.RPRequest request)
    {
        Console.WriteLine($"<Calling RPC - Call -> {methodName} with args {request.Args} on topic {topic} \n\n");
        Celte.Req.RPRequest req = new Celte.Req.RPRequest
        {
            Name = methodName,
            RespondsTo = "",
            ResponseTopic = "persistent://public/default/master.rpc", // This is the topic where the response will be sent
            RpcId = request.RpcId,
            Args = request.Args
        };
        await Master.GetInstance().pulsarProducer.ProduceMessageAsync(topic, req);
    }

    public string getUUIDFromJSON(JsonElement node)
    {
        string uuid = node.GetProperty("uuid").GetString();
        return uuid;
    }
    public void RegisterAllResponseHandlers()
    {
        if (_rpcResponseHandlers.Count > 0)
        {
            Console.WriteLine("Response handlers already registered");
            return;
        }
        // this function is used to send the spawn position of the player to the client.
        RegisterResponseHandler("__rp_getPlayerSpawnPosition", (args) =>
        {
            // string clientId = args.GetProperty("clientId").GetString();
            // string grapeId = args.GetProperty("grapeId").GetString();

            // float x = args.GetProperty("x").GetSingle();
            // float y = args.GetProperty("y").GetSingle();
            // float z = args.GetProperty("z").GetSingle();
            var nodes = Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes").Result;
            if (nodes.Count > 0)
            {
                string uuid = getUUIDFromJSON(JsonDocument.Parse(nodes[0]).RootElement);
                Console.WriteLine($"<RegisterAllResponseHandlers> Received response from __rp_getPlayerSpawnPosition with args {args} sending to uuid : {uuid} \n\n");

                string topic = $"persistent://public/default/{uuid}.rpc";

                // < RPC > Error while handling RPC: '{' is invalid after a value. Expected either ',', '}', or ']'.LineNumber: 0 | BytePositionInLine: 3.
                // var rpcArgsList = JsonDocument.Parse($"[\"{args.Args.ToString()}\"]");
                string jsonArray = JsonSerializer.Serialize(new[] { args.Args });
                var rpcArgsList = JsonDocument.Parse(jsonArray);
                Console.WriteLine($"<RegisterAllResponseHandlers> Sending response to {topic} with args {rpcArgsList.RootElement.ToString()} \n\n");

                RPRequest r = new RPRequest
                {
                    Name = "__rp_acceptNewClient",
                    RespondsTo = "",
                    ResponseTopic = "persistent://public/default/master.rpc",
                    RpcId = new Random().Next().ToString(),
                    Args = rpcArgsList.RootElement.ToString()
                };

                RPC.Call(topic, "__rp_acceptNewClient", args);
            }
            else
            {
                Console.WriteLine($"<RegisterAllResponseHandlers> No nodes found in Redis");
            }
        });
    }
}
