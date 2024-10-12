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

    public void connectNewNode(byte[] messageByte)
    {
        try
        {
            // Deserialize the message
            string message = System.Text.Encoding.UTF8.GetString(messageByte);
            Console.WriteLine("Deserialized message: " + message);

            // Assuming message is supposed to be a UUID or similar identifier
            _master.kFKProducer._uuidProducerService.OpenTopic(message);
            RPC.InvokeRemote("__rp_assignGrape", Scope.Peer(message), "leChateauDuMechant");

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