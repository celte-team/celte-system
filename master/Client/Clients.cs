using System.Reflection.Metadata;
using System.Text.Json.Nodes;

class Clients
{
    public struct ClientConnectToClusterReqBody
    {
        public string clientId { get; set; } // Client ID in the network (its RUNTIME uuid)
        public string spawnerId { get; set; } // Spawner ID in the network (its RUNTIME uuid)
        public string SessionId { get; set; }
    }


    public static (int, JsonObject) ConnectClientToCluster(ClientConnectToClusterReqBody reqBody)
    {
        Console.WriteLine("Received link request");
        string nodeId;
        try
        {
            // TODO: uniformize nameing convention to adapt with future dynamic nodes
            nodeId = RedisDb.GetSNFromSpawnerId(reqBody.spawnerId, reqBody.SessionId);
            Console.WriteLine($"Redirecting client {reqBody.clientId} to node {nodeId} for spawner {reqBody.spawnerId}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error getting nodeId from spawnerId: {ex.Message}");
            return (500, new JsonObject
            {
                ["message"] = "Failed to get nodeId from spawnerId.",
            });
        }
        if (!RPC.ConnectClientToNode(nodeId, reqBody.spawnerId, reqBody.clientId, reqBody.SessionId))
        {
            return (500, new JsonObject
            {
                ["message"] = "Failed to connect client to the server node.",
            });
        }

        return (200, new JsonObject
        {
            ["message"] = "Ok, await further instructions from the assigned node.",
            ["SessionId"] = reqBody.SessionId
        });
    }
}