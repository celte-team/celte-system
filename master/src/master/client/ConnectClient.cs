using System.Text.Json;
class ConnectClient
{
    private Master _master = Master.GetInstance();

    public struct Client
    {
        public string uuid;
    }

    public static Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public async void ConnectNewClient(string message)
    {
        string binaryData = message.Split("\"peerUuid\":\"")[1].Split("\"")[0];
        if (!_clients.ContainsKey(binaryData))
            _clients.Add(binaryData, new Client { uuid = binaryData });
        string newTopic = "persistent://public/default/" + binaryData;
        // await _master.pulsarProducer.OpenTopic(newTopic);
        try
        {
            string nodeId = await GetRandomNode();
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string clientId = root.GetProperty("peerUuid").GetString() ?? throw new InvalidOperationException("peerUuid property is missing or null");
            string uuidProcess = Guid.NewGuid().ToString();
            // const string rpcName = "__rp_getPlayerSpawnPosition";
            const string rpcName = "PeerService_call_GetPlayerSpawnPosition";


            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
            await redisClient.redisData.JSONPush("clients_try_to_connect", clientId, clientId);
            string clientIdArrayJson = JsonSerializer.Serialize(new string[] { clientId });
            Console.WriteLine($"clientIdArrayJson {clientIdArrayJson} is trying to connect.");
            _master.rpc.RegisterAllResponseHandlers();
            nodeId = "persistent://public/default/" + nodeId + ".rpc";
            Celte.Req.RPRequest request = new Celte.Req.RPRequest
            {
                Name = rpcName,
                RespondsTo = "",
                ResponseTopic = "persistent://public/default/master.rpc",
                RpcId = new Random().Next().ToString(),
                Args = clientIdArrayJson,
            };

            RPC.Call(nodeId, rpcName, request);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error connecting new client: {e.Message}");
        }
    }

    private static async Task<string> GetRandomNode()
    {
        JsonElement nodesJson = await Redis.RedisClient.GetInstance().redisData.JSONGetAll("nodes");
        var nodesId = new List<string>();
        if (nodesJson.ValueKind == JsonValueKind.Array)
        {
            foreach (JsonElement node in nodesJson.EnumerateArray())
            {

                if (node.ValueKind == JsonValueKind.String)
                {
                    using (JsonDocument nodeDoc = JsonDocument.Parse(node.GetString() ?? throw new InvalidOperationException("Node is null.")))
                    {
                        JsonElement root = nodeDoc.RootElement;

                        // Extract the "uuid" property
                        if (root.TryGetProperty("uuid", out JsonElement uuidProperty) &&
                            uuidProperty.ValueKind == JsonValueKind.String)
                        {
                            string uuid = uuidProperty.GetString() ?? throw new InvalidOperationException("UUID is null.");
                            nodesId.Add(uuid ?? throw new InvalidOperationException("Node ID is null."));
                        }
                    }
                }
                else
                {
                    Console.WriteLine("Error: Node is not a string.");
                }
            }
        }
        else
        {
            Console.WriteLine("Error: nodesJson is not a JSON array.");
        }

        if (nodesId.Count > 0)
        {
            Console.WriteLine($"\n -> Random node ID: {nodesId[new Random().Next(0, nodesId.Count)]}\n");
            return nodesId[new Random().Next(0, nodesId.Count)];
        }
        else
        {
            throw new InvalidOperationException("No valid nodes found in JSON data.");
        }
    }
}