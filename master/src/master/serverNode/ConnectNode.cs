using System;
using System.Threading;
using MessagePack;

class ConnectNode
{
    Master _master = Master.GetInstance();

    public struct Node
    {
        public string uuid;
    }
    public static Dictionary<string, Node> _nodes = new Dictionary<string, Node>();

    // public void connectNewNode(byte[] messageByte)
    public void connectNewNode(string messageByte)
    {
        try
        {
            // Deserialize the message
            // string message = System.Text.Encoding.UTF8.GetString(messageByte);
            string message = messageByte;
            Console.WriteLine("Deserialized message: " + message);

            // Assuming message is supposed to be a UUID or similar identifier
            // Open a topic for the new node
            _ = _master.pulsarProducer.OpenTopic(message);
            // _master.kFKProducer._uuidProducerService.OpenTopic(message);
            // RPC.InvokeRemote("__rp_assignGrape", Scope.Peer(message), "LeChateauDuMechant");
            // each node gets a grape so the grape assigned to this node is the one with the same index as the current node
            Console.WriteLine("Node count: " + _nodes.Count);
            string? grapeId = _master._setupConfig._grapes[_nodes.Count];
            if (grapeId != null)
            {
                RPC.InvokeRemote("__rp_assignGrape", Scope.Peer(message), grapeId);
            }
            else
            {
                Console.WriteLine("No grapes available.");
                return;
            }

            // Link node with the server
            if (!_nodes.ContainsKey(message))
            {
                _nodes.Add(message, new Node { uuid = message });
                Console.WriteLine("Node added: " + message);
            }
            else
            {
                Console.WriteLine("Node already exists: " + message);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("Error deserializing message: " + e.Message);
            Console.WriteLine("Inner exception: " + e.InnerException?.Message);
        }
    }
}