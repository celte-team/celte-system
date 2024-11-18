using Confluent.Kafka;
using System;
using MessagePack;
class ConnectClient
{
    private Master _master = Master.GetInstance();

    public struct Client
    {
        public string uuid;
    }

    // public static Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public async void connectNewClient(byte[] messageByte)
    {
        //
        int numberOfTopics = 3;
        string message = System.Text.Encoding.UTF8.GetString(messageByte);
        Console.WriteLine("New client connected to the cluster: " + message);

        // if _clients do not already contain the message, add the message to the _clients
        // check inside the 'clients' redis if the message exist:
        // Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
        // var clients = redisClient.redisData.JSONGet("clients");
        // Console.WriteLine($"Clients: {clients}");
        // if (!_clients.ContainsKey(message))
        //     _clients.Add(message, new Client { uuid = message });

        _master.kFKProducer._uuidProducerService.OpenTopic(message, numberOfTopics);
        // add the topic to the kafka consumer
        try
        {
            // select a random node from the list of nodes

            // int rand = new Random().Next(0, Redis.RedisClient.GetInstance().redisData.JSONGetAll<string>("nodes").Count);
            var nodesJson = await Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes");

            // string nodeId = ConnectNode._nodes.ElementAt(rand).Value.uuid;
            // string nodeId = Redis.RedisClient.GetInstance().redisData.JSONGetAll<string>("nodes")[rand].uuid;

            if (nodesJson == null || nodesJson.Count == 0)
            {
                throw new Exception("No nodes available.");
            }
            // Generate a random index
            int rand = new Random().Next(0, nodesJson.Count);

            // Retrieve the node ID
            string nodeId = nodesJson[rand];

            // call the function to compute the grapeId then return the values
            string clientId = message;
            string uuidProcess = Guid.NewGuid().ToString();
            const string rpcName = "__rp_getPlayerSpawnPosition";
            string masterRPC = M.Global.MasterRPC;
            Headers headers = new Headers
            {
                { "rpName", RPC.__str2bytes(rpcName) },
                { "rpcUUID", RPC.__str2bytes(uuidProcess) },
                { "peer.uuid", RPC.__str2bytes(M.Global.MasterUUID) }
            };

            await RPC.Call(rpcName, Scope.Peer(nodeId), headers, uuidProcess, async (byte[] value) =>
                {
                    Console.WriteLine($">>>>>>>>>>> Received response from getPlayerSpawnPosition: {value} <<<<<<<<<<<");
                    try
                    {
                        // Initialiser les variables de sortie
                        string grapeId = string.Empty;
                        string receivedClientId = string.Empty;
                        float x = 0, y = 0, z = 0;

                        var result = UnpackAny(value, typeof(string), typeof(string), typeof(int), typeof(int), typeof(int));
                        grapeId = (string)result.Item1[0];
                        receivedClientId = (string)result.Item1[1];
                        x = (int)result.Item1[2];
                        y = (int)result.Item1[3];
                        z = (int)result.Item1[4];
                        Console.WriteLine($"Sending response to acceptNewClient: {receivedClientId}, {grapeId}, {x}, {y}, {z}");
                        RPC.InvokeRemote("__rp_acceptNewClient", Scope.Peer(nodeId), receivedClientId, grapeId, x, y, z);
                        var jsonInfo = new
                        {
                            clientId = receivedClientId,
                            grapeId = grapeId,
                            x = x,
                            y = y,
                            z = z
                        };
                        Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
                        // redisClient.SetValue("clients", jsonSerializer.Serialize(jsonInfo));

                    }
                    catch (Exception e)
                    {
                        Console.WriteLine($"Error handling response from getPlayerSpawnPosition: {e.Message}");
                    }

                }, clientId);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error connecting new client: {e.Message}");
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