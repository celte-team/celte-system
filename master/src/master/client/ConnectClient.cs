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

    public static Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public async void connectNewClient(string message)
    {
        Console.WriteLine("New client connected to the cluster: " + message);

        // if _clients do not already contain the message, add the message to the _clients
        if (!_clients.ContainsKey(message))
            _clients.Add(message, new Client { uuid = message });

        _master.kFKProducer._uuidProducerService.OpenTopic(message);
        // send message to the client, that he will go to the node 0, send uuid chunk
        // send to the server that a client will spawn in the node 0
        try
        {
            // select a random node from the list of nodes
            int rand = new Random().Next(0, ConnectNode._nodes.Count);
            string nodeId = ConnectNode._nodes.ElementAt(rand).Value.uuid;

            // call the function to compute the grapeId then return the values
            string clientId = message;
            string uuidProcess = Guid.NewGuid().ToString();
            const string rpcName = "__rp_getPlayerSpawnPosition";
            string masterRPC = M.Global.MasterRPC;
            // Headers headers = new Headers();
            // headers.Add("rpName", RPC.__str2bytes(rpcName));
            // headers.Add("rpcUUID", RPC.__str2bytes(uuidProcess));
            // headers.Add("peer.uuid", RPC.__str2bytes(M.Global.MasterUUID));
            Headers headers = new Headers
            {
                { "rpName", RPC.__str2bytes(rpcName) },
                { "rpcUUID", RPC.__str2bytes(uuidProcess) },
                { "peer.uuid", RPC.__str2bytes(M.Global.MasterUUID) }
            };

        //     await RPC.Call(rpcName, Scope.Peer(nodeId), headers, uuidProcess, async (value) =>
        //         {
        //             // Handle the result in the callback function
        //             Console.WriteLine($">>>>>>>>>>> Received response from getPlayerSpawnPosition: {value} <<<<<<<<<<<");
        //             try {
        //                 object[] outputObjects = new object[4];

        //                 Console.WriteLine($"Received response from getPlayerSpawnPosition: {value}");
        //                 // return std::make_tuple("leChateauDuMechant", clientId, 0, 0, 0);
        //                 // ���leChateauDuMechant�+client.ceb076cf-1ccc-4f55-a370-8eaf0220ca13
        //                 // RPC.__deserialize(value, outputObjects);
        //                 string grapeId = string.Empty;
        //                 string clientId = string.Empty;
        //                 float x = 0, y = 0, z = 0;
        //                 string grapeId = string.Empty; // ou autre type approprié

        //                 // Appel de la méthode de désérialisation
        //                 // byte[] data = /* Les données reçues depuis C++ */;
        //                 DeserializeSpawnPosition(value, out grapeId, out clientId, out x, out y, out z);
        //                 // string grapeId = "leChateauDuMechant";
        //                 // string clientId = "ceb076cf-1ccc-4f55-a370-8eaf0220ca13";
        //                 // float x = 0;
        //                 // float y = 0;
        //                 // float z = 0;
        //                 Console.WriteLine($"Sending response to acceptNewClient: {clientId}, {grapeId}, {x}, {y}, {z}");
        //                 RPC.InvokeRemote("__rp_acceptNewClient", Scope.Peer(nodeId), clientId, grapeId, x, y, z);
        //             } catch (Exception e) {
        //                 Console.WriteLine($"Error handling response from getPlayerSpawnPosition: {e.Message}");
        //             }

        //         }, clientId);
        // }
        // catch (Exception e)
        // {
        //     Console.WriteLine($"Error connecting new client: {e.Message}");
        // }
        await RPC.Call(rpcName, Scope.Peer(nodeId), headers, uuidProcess, async (value) =>
            {
                Console.WriteLine($">>>>>>>>>>> Received response from getPlayerSpawnPosition: {value} <<<<<<<<<<<");
                try
                {
                    // Initialiser les variables de sortie
                    string grapeId = string.Empty;
                    string receivedClientId = string.Empty;
                    float x = 0, y = 0, z = 0;

                    // Appel de la méthode de désérialisation
                    DeserializeSpawnPosition(value, out grapeId, out receivedClientId, out x, out y, out z);

                    Console.WriteLine($"Sending response to acceptNewClient: {receivedClientId}, {grapeId}, {x}, {y}, {z}");
                    RPC.InvokeRemote("__rp_acceptNewClient", Scope.Peer(nodeId), receivedClientId, grapeId, x, y, z);
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