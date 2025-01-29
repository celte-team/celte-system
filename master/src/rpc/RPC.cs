using System.Text.Json;
using Celte.Req;
using Newtonsoft.Json;
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
// private static async void __handleRPC(string message)
// {
//     Console.WriteLine($"<__handleRPC> Received message: {message}");

//     RPRequest request;
//     try
//     {
//         request = RPRequest.Parser.ParseJson(message)
//                   ?? throw new InvalidOperationException("Failed to parse RPC request.");
//     }
//     catch (Exception ex)
//     {
//         Console.WriteLine($"<RPC> Failed to parse RPC request JSON: {ex.Message}");
//         return;
//     }

//     Console.WriteLine($"<RPC> Received RPC call {request.Name} with args {request.Args}");

//     // Handle response
//     if (!string.IsNullOrEmpty(request.RespondsTo))
//     {
//         if (_rpcResponseHandlers.TryGetValue(request.Name, out var responseHandler))
//         {
//             try
//             {
//                 // Wrap args in a JSON string if necessary
//                 var jsonArgs = WrapArgsIfNotJson(request.Args);
//                 responseHandler(jsonArgs);
//             }
//             catch (Exception ex)
//             {
//                 Console.WriteLine($"<RPC> Error handling response: {ex.Message}");
//             }
//         }
//         return;
//     }

//     // Handle regular RPC
//     if (_rpcHandlers.TryGetValue(request.Name, out var rpcHandler))
//     {
//         try
//         {
//             // Wrap args in a JSON string if necessary
//             var jsonArgs = WrapArgsIfNotJson(request.Args);
//             JsonElement? response = rpcHandler(jsonArgs);

//             if (response is null)
//             {
//                 return;
//             }

//             if (!string.IsNullOrEmpty(request.ResponseTopic)) // Caller expects a response
//             {
//                 var responseRequest = new RPRequest
//                 {
//                     Name = request.Name,
//                     RespondsTo = request.RespondsTo,
//                     ResponseTopic = "", // We don't want a response to this response
//                     RpcId = request.RpcId,
//                     Args = response.ToString()
//                 };

//                 await Master.GetInstance().pulsarProducer.ProduceMessageAsync(request.ResponseTopic, responseRequest);
//             }
//         }
//         catch (Exception ex)
//         {
//             Console.WriteLine($"<RPC> Error handling RPC: {ex.Message}");
//         }
//     }
// }

// private static JsonElement WrapArgsIfNotJson(string args)
// {
//     if (args.TrimStart().StartsWith("{") || args.TrimStart().StartsWith("["))
//     {
//         // Already valid JSON
//         return JsonDocument.Parse(args).RootElement;
//     }

//     // Treat it as a simple string and wrap it in JSON
//     var jsonString = $"\"{args}\"";
//     return JsonDocument.Parse(jsonString).RootElement;
// }

    // private static async void __handleRPC(string message)
    // {
    //     RPRequest request = RPRequest.Parser.ParseJson(message);
    //     if (request == null)
    //     {
    //         throw new InvalidOperationException("Failed to parse RPC request.");
    //     }
    //     Console.WriteLine($"<RPC> Received RP123123123123123123123C call {request.Name} with args {request.Args}");
    //     if (request.RespondsTo != "")
    //     { // this is a response, not a call
    //         if (_rpcResponseHandlers.TryGetValue(request.Name, out Action<JsonElement>? v))
    //         {
    //             v(request.Args);
    //             Console.WriteLine($"<RPC> Reccascawefiugi8pyfgtyufutyfutyfu");
    //             // v(JsonDocument.Parse(request.Args).RootElement);
    //         }
    //         return;
    //     }
    //     Console.WriteLine($"<RPC> Received RPC call {request.Name} with Args {request.Args}");
    //     if (_rpcHandlers.TryGetValue(request.Name, out Func<JsonElement, JsonElement?>? value))
    //     {
    //         JsonElement? response = value(JsonDocument.Parse(request.Args).RootElement);
    //         if (response is null)
    //         {
    //             return;
    //         }
    //         if (request.ResponseTopic != "") // if empty, the caller doesn't want a response
    //         {
    //             var responseRequest = new RPRequest
    //             {
    //                 Name = request.Name,
    //                 RespondsTo = request.RespondsTo,
    //                 ResponseTopic = "", // we don't want a response to this response
    //                 RpcId = request.RpcId,
    //                 Args = response.ToString()
    //             };
    //             await Master.GetInstance().pulsarProducer.ProduceMessageAsync(request.ResponseTopic, responseRequest);
    //         }
    //     }
    // }

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
                    Console.WriteLine($"<RPC> Reccascawefiugi8pyfgtyufutyfutyfu");
                    // v(JsonDocument.Parse(request.Args).RootElement);
                }
                return;
            }

        if (!string.IsNullOrEmpty(request.RespondsTo))  // This is a response, not a call
        {
            if (_rpcResponseHandlers.TryGetValue(request.Name, out Action<Celte.Req.RPRequest> responseHandler))
            {
                responseHandler(request);
                Console.WriteLine("<RPC> Handled response");
            }
            return;
        }

        // Handle normal RPC call
        Console.WriteLine($"<RPC> Received RPC call::::: {request.Name} with args {request.Args} \n\n");

        // if (_rpcHandlers.TryGetValue(request.Name, out Func<JsonElement, JsonElement?> handler))
        // {
            // Pass the request.Args as string to the handler (or you can customize it if needed)
            // var jsonArgs = JsonDocument.Parse(request.Args).RootElement;
            // if (jsonArgs.ValueKind == JsonValueKind.Undefined)
            // {
            //     Console.WriteLine("<RPC> Invalid JSON args");
            //     return;
            // }

            Console.WriteLine($"!string.IsNullOrEmpty(request.ResponseTopic) ::: {request.ResponseTopic} \n\n");

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
                Console.WriteLine($"<RPC GetInstance> Sending response to {request.ResponseTopic} \n\n");
                // Send the response to the corresponding topic
                await Master.GetInstance().pulsarProducer.ProduceMessageAsync(request.ResponseTopic, responseRequest);
            }
        // }
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

    // public async static void Call(string topic, string methodName, JsonElement args)
    // {
    //     int rpcId = new Random().Next();
    //     var request = new RPRequest
    //     {
    //         name = methodName,
    //         respondsTo = "",
    //         responseTopic = "persistent://public/default/master.rpc",
    //         rpcId = rpcId.ToString(),
    //         args = args
    //     };
    //     Console.WriteLine($"<Calling RPC> {methodName} with args {args} on topic {topic}");
    //     await Master.GetInstance().pulsarProducer.ProduceMessageAsync(topic, request.ToJson());
    // }

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
                // var rpcArgs = JsonSerializer.Serialize(new { clientId, grapeId, x, y, z });
                // var rpcArgsList = JsonDocument.Parse($"[\"{clientId}\",\"{grapeId}\",{x},{y},{z}]");
                // Console.WriteLine($"<Calling RPC> __rp_acceptNewClient with args {rpcArgs} on topic {topic}");
                // RPC.Call(topic, "__rp_acceptNewClient", rpcArgsList.RootElement);
                RPC.Call(topic, "__rp_acceptNewClient", args);

            }
        });
    }
}
