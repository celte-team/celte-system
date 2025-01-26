using System.Text.Json;


public struct RPRequest
{
    public string name { get; set; }
    public string respondsTo { get; set; }
    public string responseTopic { get; set; }
    public string rpcId { get; set; }
    public JsonElement args { get; set; }

    public string ToJson()
    {
        return JsonSerializer.Serialize(this);
    }

    public static RPRequest FromJson(string jsonString)
    {
        return JsonSerializer.Deserialize<RPRequest>(jsonString);
    }
}

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
    private static Dictionary<string, Action<JsonElement>> _rpcResponseHandlers = new Dictionary<string, Action<JsonElement>>();

    public static void Register(string methodName, Func<JsonElement, JsonElement?> handler)
    {
        _rpcHandlers[methodName] = handler;
    }

    public static void RegisterResponseHandler(string methodName, Action<JsonElement> handler)
    {
        _rpcResponseHandlers[methodName] = handler;
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
        RPRequest request = RPRequest.FromJson(message);
        if (request.respondsTo != "")
        { // this is a response, not a call
            if (_rpcResponseHandlers.TryGetValue(request.name, out Action<JsonElement>? v))
            {
                v(request.args);
            }
            return;
        }
        Console.WriteLine($"<RPC> Received RPC call {request.name} with args {request.args}");
        if (_rpcHandlers.TryGetValue(request.name, out Func<JsonElement, JsonElement?>? value))
        {
            JsonElement? response = value(request.args);
            if (response is null)
            {
                return;
            }
            if (request.responseTopic != "") // if empty, the caller doesn't want a response
            {
                var responseRequest = new RPRequest
                {
                    name = request.name,
                    respondsTo = request.respondsTo,
                    responseTopic = "", // we don't want a response to this response
                    rpcId = request.rpcId,
                    args = response.Value
                };
                await Master.GetInstance().pulsarProducer.ProduceMessageAsync(request.responseTopic, responseRequest.ToJson());
            }
        }
    }

    /// <summary>
    /// calls a method on the given remote topic, and *does not* wait for a response
    /// The request object must be serializable to JSON.
    /// </summary>
    public async static void Call(string topic, string methodName, JsonElement args)
    {
        int rpcId = new Random().Next();
        var request = new RPRequest
        {
            name = methodName,
            respondsTo = "",
            responseTopic = "persistent://public/default/master.rpc",
            rpcId = rpcId.ToString(),
            args = args
        };
        Console.WriteLine($"<Calling RPC> {methodName} with args {args} on topic {topic}");
        await Master.GetInstance().pulsarProducer.ProduceMessageAsync(topic, request.ToJson());
        Console.WriteLine($"RPC call {methodName} sent to {topic}");
    }

    public string getUUIDFromJSON(JsonElement node)
    {
        string uuid = node.GetProperty("uuid").GetString();
        Console.WriteLine($"<Calling RPC> getUUIDFromJSON with uuid {uuid}");
        return uuid;
    }

    public void RegisterAllResponseHandlers()
    {
        if (_rpcResponseHandlers.Count > 0)
        {
            Console.WriteLine("Response handlers already registered");
            return;
        }
        Console.WriteLine("Registering response handlers");
        // this function is used to send the spawn position of the player to the client.
        RegisterResponseHandler("__rp_getPlayerSpawnPosition", (args) =>
        {
            string clientId = args.GetProperty("clientId").GetString();
            string grapeId = args.GetProperty("grapeId").GetString();

            float x = args.GetProperty("x").GetSingle();
            float y = args.GetProperty("y").GetSingle();
            float z = args.GetProperty("z").GetSingle();
            var nodes = Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes").Result;
            if (nodes.Count > 0)
            {
                string uuid = getUUIDFromJSON(JsonDocument.Parse(nodes[0]).RootElement);
                string topic = $"persistent://public/default/{uuid}.rpc";
                var rpcArgs = JsonSerializer.Serialize(new { clientId, grapeId, x, y, z });
                var rpcArgsList = JsonDocument.Parse($"[\"{clientId}\",\"{grapeId}\",{x},{y},{z}]");
                Console.WriteLine($"<Calling RPC> __rp_acceptNewClient with args {rpcArgs} on topic {topic}");
                RPC.Call(topic, "__rp_acceptNewClient", rpcArgsList.RootElement);
            }
        });
    }
}
