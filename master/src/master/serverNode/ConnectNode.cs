using System.Text.Json;
using Confluent.Kafka.Admin;

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
        NodeStruct node = new NodeStruct
        {
            uuid = uuid,
            grapes = getGrape()
        };

        // Serialize the node
        string nodeJson = JsonSerializer.Serialize(node);

        // Push to Redis
        return await Redis.RedisClient.GetInstance().redisData.JSONPush("nodes", uuid, nodeJson);
    }


    public List<string> getGrape()
    {
        Dictionary<string, object>? yamlObject = _master._setupConfig.GetYamlObjectConfig();
        var grapes = JsonSerializer.Serialize(yamlObject["grapes"]);
        List<string> grapesStr = JsonSerializer.Deserialize<List<string>>(grapes);
        if (grapes == null || grapesStr.Count == 0)
        {
            throw new InvalidOperationException("No grapes found in the configuration.");
        }
        return grapesStr;
    }
    public async Task AssignGrapeToNodeAsync(string node)
    {
        Dictionary<string, object>? yamlObject = _master._setupConfig.GetYamlObjectConfig();
        if (yamlObject == null || !yamlObject.ContainsKey("grapes"))
        {
            throw new InvalidOperationException("Grapes configuration is missing.");
        }
        var grapes = JsonSerializer.Serialize(yamlObject["grapes"]);
        List<string> grapesStr = JsonSerializer.Deserialize<List<string>>(grapes);
        if (grapes == null || grapesStr.Count == 0)
        {
            throw new InvalidOperationException("No grapes found in the configuration.");
        }

        // RPC call to assign grape
        var rpcNode = "persistent://public/default/" + node + ".rpc";
        var assignGrape = JsonDocument.Parse($"[\"{grapesStr[0]}\"]").RootElement;
        Console.WriteLine("Assigning grape to node: " + assignGrape);
        RPC.Call(rpcNode, "__rp_assignGrape", assignGrape);
        // update the redis node with the grape
        NodeStruct nodeStruct = new NodeStruct { uuid = node, grapes = grapesStr };
        await Redis.RedisClient.GetInstance().redisData.UpdateGrapeList("nodes", node, JsonSerializer.Serialize(nodeStruct));
    }

    public async void connectNewNode(string message)
    {
        try
        {
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string binaryData = root.GetProperty("binaryData").GetString() ?? throw new InvalidOperationException("binaryData property is missing or null");
            _ = _master.pulsarProducer.OpenTopic(binaryData);
            Console.WriteLine("Node creating...: " + binaryData);
            var rpcNode = "persistent://public/default/" + binaryData + ".rpc";
            var assignGrape = JsonDocument.Parse($"[\"LeChateauDuMechant\"]").RootElement;
            Console.WriteLine("Assigning grape to node: " + assignGrape + " rpcNode: " + rpcNode);
            RPC.Call(rpcNode, "__rp_assignGrape", assignGrape);
            await AddNode(binaryData);
            Console.WriteLine("New node");
        }
        catch (Exception e)
        {
            Console.WriteLine("Error deserializing message: " + e.Message);
        }
    }
}
