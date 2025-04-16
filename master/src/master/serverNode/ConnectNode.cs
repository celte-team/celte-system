using System.Text.Json;

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
        // Try to acquire a distributed lock
        var lockKey = "grape_assignment_lock";
        var lockValue = Guid.NewGuid().ToString();
        var lockTimeout = TimeSpan.FromSeconds(1); // Lock timeout of 1 second
        var redis = Redis.RedisClient.GetInstance().redisData;

        try
        {
            // Acquire lock using SET NX with expiry
            var acquired = await redis.AcquireLock(lockKey, lockValue, lockTimeout);

            if (!acquired)
            {
                Console.WriteLine("Failed to acquire lock for grape assignment");
                return false;
            }

            // Critical section - FIRST check if the node already exists to avoid duplicate assignments
            var existingNodes = await GetNodes();
            if (existingNodes != null && existingNodes.Any(nodeJson =>
            {
                try
                {
                    using var doc = JsonDocument.Parse(nodeJson);
                    return doc.RootElement.GetProperty("uuid").GetString() == uuid;
                }
                catch
                {
                    return false;
                }
            }))
            {
                Console.WriteLine($"Node {uuid} already exists, skipping grape assignment");
                return false;
            }

            // Now get and assign grape - this must be inside the critical section
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

            // Serialize the node and push to Redis - still inside critical section
            string nodeJson = JsonSerializer.Serialize(node);
            return await redis.JSONPush("nodes", uuid, nodeJson);
        }
        finally
        {
            // Release the lock
            await redis.ReleaseLock(lockKey, lockValue);
        }
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

            // Try to add the node with retries
            int maxRetries = 3;
            int retryDelayMs = 1000; // 1 second delay between retries
            bool success = false;

            for (int i = 0; i < maxRetries && !success; i++)
            {
                if (i > 0)
                {
                    Console.WriteLine($"Retry {i} to add node {binaryData}");
                    await Task.Delay(retryDelayMs);
                }

                success = await AddNode(binaryData);
            }

            if (!success)
            {
                Console.WriteLine($"Failed to add node {binaryData} after {maxRetries} attempts");
                return;
            }

            // Get the assigned grape from Redis to ensure we use the correct one
            var nodes = await GetNodes();
            string assignedGrape = null;
            foreach (var nodeJson in nodes)
            {
                try
                {
                    using var doc = JsonDocument.Parse(nodeJson);
                    var node = doc.RootElement;
                    if (node.GetProperty("uuid").GetString() == binaryData)
                    {
                        var grapes = node.GetProperty("grapes").EnumerateArray();
                        if (grapes.Any())
                        {
                            assignedGrape = grapes.First().GetString();
                            break;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error parsing node JSON: {ex.Message}");
                }
            }

            if (assignedGrape == null)
            {
                Console.WriteLine("No grape was assigned to the node");
                return;
            }

            var assignGrape = JsonDocument.Parse($"[\"{assignedGrape}\"]").RootElement;
            Console.WriteLine("Assigning grape " + assignedGrape + " to node " + binaryData);

            //Protobuf message RPRequest
            Celte.Req.RPRequest request = new Celte.Req.RPRequest
            {
                Name = "PeerService_call_AssignGrape",
                RespondsTo = "",
                ResponseTopic = "persistent://public/default/master.rpc",
                RpcId = new Random().Next().ToString(),
                Args = assignGrape.ToString(),
                ErrorStatus = false,
            };

            RPC.Call(rpcNode, "PeerService_call_AssignGrape", request);
            await AddNode(binaryData);
        }
        catch (Exception e)
        {
            Console.WriteLine("Error deserializing message: " + e.Message);
        }
    }
}
