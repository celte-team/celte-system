class ConnectClient
{
    private Master _master = Master.GetInstance();

    public struct Client {
        public string uuid;
    }

    private Dictionary<string, Client> _clients = new Dictionary<string, Client>();

    public void connectNewClient(string message)
    {
        Console.WriteLine("Welcome!!!!!!!!!!!!!! new entry.");
        Console.WriteLine($"Message: {message}");
        _master.kFKProducer._uuidProducerService.ProduceUUID(1);

        // if _clients do not already contain the message, add the message to the _clients
        if (!_clients.ContainsKey(message))
            _clients.Add(message, new Client { uuid = message });

        int position = 0;

        // send message to the client, that he will go to the node 0, send uuid chunk
        // send to the server that a client will spawn in the node 0
    }
}