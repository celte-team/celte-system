using MessagePack;
using System.Text.Json;
class ConnectClient
{
    private Master _master = Master.GetInstance();

    public struct Client
    {
        public string uuid;
    }

    public static Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public async void connectNewClient(string message)
    {
        Console.WriteLine("New client connected to the cluster: " + message);
        if (!_clients.ContainsKey(message))
            _clients.Add(message, new Client { uuid = message });

        _ = _master.pulsarProducer.OpenTopic(message);
        try
        {
            string nodeId = await GetRandomNode();
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string clientId = root.GetProperty("peerUuid").GetString() ?? throw new InvalidOperationException("peerUuid property is missing or null");
            string uuidProcess = Guid.NewGuid().ToString();
            const string rpcName = "__rp_getPlayerSpawnPosition";

            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
            await redisClient.redisData.JSONPush("clients_try_to_connect", clientId, clientId);

            _master.rpc.RegisterAllResponseHandlers();
            nodeId = "persistent://public/default/" + nodeId + ".rpc";
            JsonElement argsElement = JsonDocument.Parse($"[\"{clientId}\"]").RootElement;
            RPC.Call(nodeId, rpcName, argsElement);
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
                    using (JsonDocument nodeDoc = JsonDocument.Parse(node.GetString()))
                    {
                        JsonElement root = nodeDoc.RootElement;

                        // Extract the "uuid" property
                        if (root.TryGetProperty("uuid", out JsonElement uuidProperty) &&
                            uuidProperty.ValueKind == JsonValueKind.String)
                        {
                            string uuid = uuidProperty.GetString();
                            nodesId.Add(uuid);
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

    static Tuple<object[]> UnpackAny(byte[] serializedData, params Type[] types)
    {
        var deserializedData = MessagePackSerializer.Deserialize<object>(serializedData);
        Console.WriteLine($"Deserialized data: {deserializedData}");
        if (deserializedData is object[] array && array.Length == 1 && array[0] is object[] innerArray)
        {
            Console.WriteLine($"Inner array: {innerArray}");
            if (innerArray.Length != types.Length)
            {
                throw new InvalidOperationException("The number of types provided does not match the number of elements in the serialized data.");
            }

            object[] result = new object[types.Length];
            for (int i = 0; i < types.Length; i++)
            {
                result[i] = Convert.ChangeType(innerArray[i], types[i]);
            }
            return Tuple.Create(result);
        }
        throw new InvalidOperationException("Invalid serialized data format.");
    }
}