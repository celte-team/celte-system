class ConnectClient
{
    private Master _master = Master.GetInstance();

    public struct Client
    {
        public string uuid;
    }

    public static Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public void connectNewClient(string message)
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
            string nodeId = ConnectNode._nodes.ElementAt(0).Value.uuid; // TODO: @Laurent, this is temporary, we need to implement the logic to assign the client to the node

            Console.WriteLine("Available nodes are:");
            foreach (var node in ConnectNode._nodes)
            {
                Console.WriteLine(node.Value.uuid);
            }

            string clientId = message;
            string grapeId = "leChateauDuMechant"; // tmp, Laurent will implement the logic
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;


            RPC.InvokeRemote("__rp_acceptNewClient", Scope.Peer(nodeId), clientId, grapeId, x, y, z);
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error connecting new client: {e.Message}");
        }
    }
}