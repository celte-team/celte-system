using System;
using System.Threading;

class ConnectNode
{
    Master _master = Master.GetInstance();

    public struct Node
    {
        public string uuid;
    }
    public static Dictionary<string, Node> _nodes = new Dictionary<string, Node>();

    public void connectNewNode(string message)
    {
        Console.WriteLine("New node connected to the cluster: " + message);
        // ? TODO:  @Clement does the commented line below create a new uuid ? thought the message was the uuid
        // _master.kFKProducer._uuidProducerService.ProduceUUID(1);

        // create a topic form the UUID and assign the node to the topic
        _master.kFKProducer._uuidProducerService.OpenTopic(message);
        RPC.InvokeRemote("__rp_assignGrape", Scope.Peer(message), "LeChateauDuMechant");// only one grape and one node for now but TODO: assign a grape to the node using some actual logic

        // link node with the server
        if (!_nodes.ContainsKey(message))
            _nodes.Add(message, new Node { uuid = message });
    }
}