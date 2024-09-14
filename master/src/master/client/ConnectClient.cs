class ConnectClient
{
    private Master _master = Master.GetInstance();

    public void connectNewClient(string message)
    {
        Console.WriteLine("Welcome!!!!!!!!!!!!!! new entry.");
        Console.WriteLine($"Message: {message}");
        // get the list of all the uuid, and assign the client to the uuid
        var uuid = _master.kFKProducer._uuidProducerService._uuids
            .Where(uuid => uuid.Value == null)
            .FirstOrDefault();
        // if (uuid.Value != null)
        // {
        //     // assign the client to the uuid
        //     uuid.Value = message;
        // }
        // link client avec le serveur
    }
}