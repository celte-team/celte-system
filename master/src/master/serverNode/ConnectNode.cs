using System;
using System.Text.Json;
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

    public async void connectNewNode(string message)
    {
        try
        {
            JsonDocument messageJson = JsonDocument.Parse(message);
            JsonElement root = messageJson.RootElement;
            string binaryData = root.GetProperty("binaryData").GetString() ?? throw new InvalidOperationException("binaryData property is missing or null");
            _ = _master.pulsarProducer.OpenTopic(binaryData);
            // check if the node already exists
            if (Redis.RedisClient.GetInstance().redisData.JSONExists("nodes", binaryData))
            {
                await AddNode(binaryData);
            }
            else
            {
                Console.WriteLine("Node already exists: " + binaryData);
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("Error deserializing message: " + e.Message);
        }
    }
}