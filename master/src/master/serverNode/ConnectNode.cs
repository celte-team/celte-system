using System.Text.Json;
using Celte.Req;
class ConnectNode
{
    Master _master = Master.GetInstance();

    // create a structure with node uuid and grapes list
    public class NodeStruct
    {
        public string uuid { get; set; }
        public List<string> grapes { get; set; }
    }

    public string ToString(NodeStruct node)
    {
        return JsonSerializer.Serialize(node);
    }

    /// <summary>
    /// Get all nodes from redis
    /// </summary>
    /// <returns>
    /// return a System.Collections.Generic.List`1[System.String]
    /// you can use this to iterate over the list of nodes
    /// </returns>
    public async Task<List<string>> GetNodes()
    {
        return await Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes");
    }

    public async Task<bool> AddNode(string uuid)
    {
        // Create a new node
        Grape grape = new Grape();
        string grapeStr = await grape.ReturnNextGrape();
        List<string> grapes = new List<string>();
        if (grapeStr == null)
        {
            grapes = new List<string>();
        }
        else
        {
            grapes = new List<string> { grapeStr };
        }
        NodeStruct node = new NodeStruct
        {
            uuid = uuid,
            grapes = grapes,
        };

        // Serialize the node
        string nodeJson = JsonSerializer.Serialize(node);
        // Push to Redis
        return await Redis.RedisClient.GetInstance().redisData.JSONPush("nodes", uuid, nodeJson);
    }

    public async void connectNewNode(string message)
    {
        try
        {
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string binaryData = root.GetProperty("binaryData").GetString() ?? throw new InvalidOperationException("binaryData property is missing or null");
            _ = _master.pulsarProducer.OpenTopic(binaryData);
            var rpcNode = "persistent://public/default/" + binaryData + ".rpc";
            string grapeToSpawn = await new Grape().ReturnNextGrape();

            if (grapeToSpawn == null)
            {
                Console.WriteLine("No grapes available to spawn");
                return;
            }
            var assignGrape = JsonDocument.Parse($"[\"{grapeToSpawn}\"]").RootElement;
            Console.WriteLine("Assigning grape " + grapeToSpawn + " to node " + binaryData);


            //Protobuf message RPRequest
            Celte.Req.RPRequest request = new Celte.Req.RPRequest
            {
                Name = "__rp_assignGrape",
                RespondsTo = "",
                ResponseTopic = "persistent://public/default/master.rpc",
                RpcId = new Random().Next().ToString(),
                Args = grapeToSpawn
            };

            RPC.Call(rpcNode, "__rp_assignGrape", request);
            await AddNode(binaryData);
        }
        catch (Exception e)
        {
            Console.WriteLine("Error deserializing message: " + e.Message);
        }
    }
}
