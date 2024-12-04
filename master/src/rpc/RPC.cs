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

    public static void InitConsumer()
    {
        Master.GetInstance().pulsarConsumer.CreateConsumer(new SubscribeOptions
        {
            Topics = "persistent://public/default/master.rpc",
            SubscriptionName = "master.rpc",
            Handler = (consumer, message) => __handleRPC(message)
        });
    }

    /// <summary>
    /// Handles an incoming RPC message. If the rpc expects a response, it will be sent back.
    /// </summary>
    /// <param name="message"></param>
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
    public async static void Call(string topic, string methodName, JsonElement Args)
    {
        var request = new RPRequest
        {
            name = methodName,
            respondsTo = "",
            responseTopic = "pulsar://persistent/default/master.rpc",
            rpcId = Guid.NewGuid().ToString(),
            args = Args
        };
        await Master.GetInstance().pulsarProducer.ProduceMessageAsync(topic, request.ToJson());
    }

    public void registerAllReponseHandlers()
    {
        if (_rpcResponseHandlers.Count > 0)
        {
            Console.WriteLine("Response handlers already registered");
            return;
        }
        RegisterResponseHandler("__rp_getPlayerSpawnPosition", (args) =>
        {
            Console.WriteLine("Received response from getPlayerSpawnPosition: " + args);
            string dataSpawnPosition = JsonSerializer.Serialize(new
            {
                clientId = args.GetProperty("clientId").GetString(),
                grapeId = args.GetProperty("grapeId").GetString(),
                x = args.GetProperty("x").GetSingle(),
                y = args.GetProperty("y").GetSingle(),
                z = args.GetProperty("z").GetSingle()
            });
            _ = Master.GetInstance().pulsarProducer.ProduceMessageAsync(args.GetProperty("clientId").GetString(), dataSpawnPosition);
        });
    }
}