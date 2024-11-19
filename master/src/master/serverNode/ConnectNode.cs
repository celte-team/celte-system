using System;
using System.Threading;
using MessagePack;

class ConnectNode
{
    Master _master = Master.GetInstance();

    /// <summary>
    /// Get all nodes from redis
    /// </summary>
    /// <returns>
    /// return a System.Collections.Generic.List`1[System.String]
    /// you can use this to iterate over the list of nodes
    /// </returns>
    public async Task<List<string>> GetNodes()
    {
        return await Redis.RedisClient.GetInstance().redisData.JSONGetAll<List<string>>("nodes");
    }

    // public async void AddNode(string uuid)
    public async Task<bool> AddNode(string uuid)
    {
        await Redis.RedisClient.GetInstance().redisData.JSONPush("nodes", uuid, uuid);
        return true;
    }

    public async void connectNewNode(byte[] messageByte)
    {
        try
        {
            // Deserialize the message
            string message = System.Text.Encoding.UTF8.GetString(messageByte);
            Console.WriteLine("Deserialized message: " + message);

            await _master.kFKProducer._uuidProducerService.OpenTopic(message, 3);
            Console.WriteLine("Topic opened: " + message);
            RPC.InvokeRemote("__rp_assignGrape", Scope.Peer(message), "leChateauDuMechant");
            await AddNode(message);
            // Link node with the server

            if (!Redis.RedisClient.GetInstance().redisData.JSONExists("nodes", message))
            {
                // _nodes.Add(message, new Node { uuid = message });
                Redis.RedisClient.GetInstance().redisData.JSONPush("nodes", message, message);
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
        }
    }
}