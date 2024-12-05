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
            // Get the list of nodes from Redis
            string nodeId = await GetRandomNode();
            // peerUuid
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string clientId = root.GetProperty("peerUuid").GetString() ?? throw new InvalidOperationException("peerUuid property is missing or null");
            string uuidProcess = Guid.NewGuid().ToString();
            const string rpcName = "__rp_getPlayerSpawnPosition";

            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
            await redisClient.redisData.JSONPush("clients_try_to_connect", clientId, clientId);

            _master.rpc.registerAllReponseHandlers();
            string jsonArgsSpawnPosition = JsonSerializer.Serialize(new {
                // name = rpcName,
                clientId,
                grapeId = nodeId,
                // rpcId = uuidProcess,
                // responseTopic = "persistent://public/default/master.rpc",
                // respondsTo = "",
            });

            JsonDocument jsonDocument = JsonDocument.Parse(jsonArgsSpawnPosition);
            JsonElement jsonElement = jsonDocument.RootElement;
            nodeId = "persistent://public/default/" + nodeId + ".rpc";
            RPC.Call(nodeId, rpcName, jsonElement);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error connecting new client: {e.Message}");
        }
    }

    private async Task<string> GetRandomNode()
    {
        // fetch the list of nodes from Redis
        var nodesJson = await Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes");
        if (nodesJson == null || nodesJson.Count == 0)
        {
            throw new Exception("No nodes available.");
        }
        // Generate a random index
        int rand = new Random().Next(0, nodesJson.Count);
        // // Retrieve the node ID
        string nodeId = nodesJson[rand];
        return nodeId;
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


    private void DeserializeSpawnPosition(byte[] value, out string grapeId, out string clientId, out float x, out float y, out float z)
    {
        grapeId = "";
        clientId = "";
        x = y = z = 0.0f;

        try
        {
            object[] deserializedData = MessagePackSerializer.Deserialize<object[]>(value);
            Console.WriteLine($"Deserialized objects: {string.Join(", ", deserializedData)}");
            Console.WriteLine($"Deserialized Length: {deserializedData.Length}");
            Console.WriteLine($"Deserialized objects tyeps: {deserializedData}");
            grapeId = deserializedData[0] as string ?? "";
            clientId = deserializedData[1] as string ?? "";
            x = Convert.ToSingle(deserializedData[2]);
            y = Convert.ToSingle(deserializedData[3]);
            z = Convert.ToSingle(deserializedData[4]);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Deserialization error: {ex.Message}");
        }
    }
}