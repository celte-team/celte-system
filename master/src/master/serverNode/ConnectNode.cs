using System;
using System.Threading;

class ConnectNode
{
    Master _master = Master.GetInstance();

    public struct Node
    {
        public string uuid;
    }
    private Dictionary<string, Node> _nodes = new Dictionary<string, Node>();

    public void connectNewNode(string message)
    {
        // message = uuid
        Console.WriteLine("Welcome!!!!!!!!!!!!!! new entry.");
        Console.WriteLine($"Message: {message}");
        _master.kFKProducer._uuidProducerService.ProduceUUID(1);
        // create a topic form the UUID and assign the node to the topic
        _master.kFKProducer._uuidProducerService.OpenTopic(message);
        // link node with the server
        if (!_nodes.ContainsKey(message))
            _nodes.Add(message, new Node { uuid = message });
    }
}