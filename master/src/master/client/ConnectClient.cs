using Confluent.Kafka;
using System;

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
            var uuidProcess = Guid.NewGuid().ToString();
            const string rpcName = "__rp_getPlayerSpawnPosition";
            const string masterRPC = "master";
            Headers headers = new Headers();
            headers.Add("rpcName", RPC.__str2bytes(rpcName));
            headers.Add("rpcUUID", RPC.__str2bytes(uuidProcess));
            headers.Add("peer.uuid", RPC.__str2bytes(masterRPC));

            RPC.Call(rpcName, Scope.Peer(nodeId), headers, async (value) =>
                {
                    // Handle the result in the callback function
                    Console.WriteLine("Callback executed with result: " + value);
                        string valueString = RPC.__deserialize(value);
                        string grapeId = valueString["grapeId"];
                        float x = valueString["x"];
                        float y = valueString["y"];
                        float z = valueString["z"];
                        // value["answer"]
                        RPC.InvokeRemote("__rp_acceptNewClient", Scope.Peer("answer"), clientId, grapeId, x, y, z);
                }, clientId);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error connecting new client: {e.Message}");
        }
    }
}