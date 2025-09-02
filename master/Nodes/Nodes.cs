using System.Text.Json.Nodes;
using System.Text.Json;

/// <summary>
/// This file contains the implementation of the connection and creation algorithms for nodes in the network.
///
/// Algorithm Overview:
///
/// 1. **Node Connection (AcceptNode):**
///    - A node attempting to connect must provide its ID, parent ID, and readiness status.
///    - The system acquires a Redis-based lock on the node ID to ensure no concurrent modifications.
///    - If the node is not ready or unauthorized, the connection is rejected.
///    - A node is considered authroized if there has been a request for it to be created and its information is already present in the Redis database.
///    - If valid, the node is accepted, and its details are returned.
///
/// 2. **Node Creation (CreateNode):**
///    - A new node is created under a specified parent node.
///    - The child node ID is deterministically generated using the parent ID and a Redis counter to ensure uniqueness.
///    - The node is initialized with a default "not ready" status and scheduled for further setup.
///
/// These algorithms ensure consistency, determinism, and proper synchronization in the distributed network.
/// </summary>

class Nodes
{

    /* -------------------------------------------------------------------------- */
    /*                             ACCEPT NODE REQUEST                            */
    /* -------------------------------------------------------------------------- */
    #region AcceptNode
    public struct AcceptNodeReqBody
    {
        public string Id { get; set; }
        public string Pid { get; set; }
        public string SessionId { get; set; }
        public bool Ready { get; set; }
    }

    public struct NodeInfo
    {
        public string Id { get; set; }
        public string Pid { get; set; }
        public string Ready { get; set; }
        public string Payload { get; set; }
        public string SessionId { get; set; }

    }


    /// <summary>
    /// Wrapper for the acceptNodeImpl method. This method acquires a lock on the node ID in redis before calling acceptNodeImpl.
    /// If the lock cannot be acquired, it will throw an exception.
    /// </summary>
    /// <param name="reqBody"></param>
    /// <exception cref="Exception"></exception>
    public static (int, JsonObject) AcceptNode(AcceptNodeReqBody reqBody)
    {
        var dlm = new Redlock.CSharp.Redlock(RedisDb.Connection);
        var resourceName = $"lock:{reqBody.Id}";

        Redlock.CSharp.Lock lockObject;
        if (!dlm.Lock(resourceName, new TimeSpan(0, 0, 10), out lockObject))
        {
            throw new RetryableException("Failed to acquire lock");
        }

        var (status, body) = acceptNodeImpl(reqBody);
        dlm.Unlock(lockObject);
        return (status, body);
    }

    /// <summary>
    /// Accepts a new node in the network. If the request is invalid (unauthorized or malformed), it will throw an exception.
    /// To call this method, you must first acquire a lock on the node ID in redis. (See AcceptNode method)
    /// </summary>
    /// <param name="reqBody"></param>
    /// <exception cref="Exception"></exception>
    /// <returns>
    ///     - The first item is the status code (200 for success, 400 for bad request, 500 for server error).
    ///     - The second item is a JsonObject containing the response data.
    /// </returns>
    private static (int, JsonObject) acceptNodeImpl(AcceptNodeReqBody reqBody)
    {
        // If node is not ready, return 400
        if (!reqBody.Ready)
        {
            return (400, new JsonObject
            {
                ["error"] = "Node is not ready"
            });
        }


        // Try to get the existing node data from Redis. Error if does not exist. (unauthorized connection)
        string? existingNodeData = RedisDb.GetHashField("nodes", reqBody.Id);
        if (existingNodeData == null)
        {
            return (400, new JsonObject
            {
                ["error"] = "Unauthorized connection to the Celte cluster"
            });
        }

        // If the new is not already known by the cluster, treat it as an unauthorized connection.
        NodeInfo existingNodeInfo;
        try
        {
            existingNodeInfo = JsonSerializer.Deserialize<NodeInfo>(existingNodeData);
        }
        catch (JsonException ex)
        {
            return (500, new JsonObject
            {
                ["error"] = "Failed to deserialize existing node data: " + ex.Message
            });
        }



        return (200, new JsonObject
        {
            ["message"] = "Node accepted",
            ["node"] = new JsonObject
            {
                ["id"] = existingNodeInfo.Id,
                ["pid"] = existingNodeInfo.Pid,
                ["ready"] = existingNodeInfo.Ready,
                ["payload"] = existingNodeInfo.Payload,
                ["sessionId"] = existingNodeInfo.SessionId
            }
        });
    }

    #endregion
    /* -------------------------------------------------------------------------- */
    /*                             CREATE NODE REQUEST                            */
    /* -------------------------------------------------------------------------- */
    #region CreateNode

    public struct CreateNodeReqBody
    {
        public string parentId { get; set; }
        public string payload { get; set; }
        public string SessionId { get; set; }
    }

    /// <summary>
    /// Create a new node in the network.
    /// The id of the new node is generated using the parentId and a Redis counter.
    /// If the parentId does not start with 'sn-, it is assumed to be a request from another process and the id will be 'sn-<parentId>'.
    /// Example:
    /// - parentId = LeChateauDuMechant -> id = sn-LeChateauDuMechant
    /// - parentId = sn-LeChateauDuMechant -> id = sn-LeChateauDuMechant-1
    /// - parentId = sn-LeChateauDuMechant-1 -> id = sn-LeChateauDuMechant-2
    /// </summary>
    /// <param name="reqBody"></param>
    /// <returns></returns>
    public static (int, JsonObject) CreateNode(CreateNodeReqBody reqBody)
    {
        string id;
        if (!reqBody.parentId.StartsWith("sn-"))
        {
            id = reqBody.SessionId + "-sn-" + reqBody.parentId;
        }
        else
        {
            // we generate a deterministic ID using a Redis counter to keep track of the filiation
            string counterKey = $"counter:{reqBody.parentId}";
            long counter = RedisDb.Database.StringIncrement(counterKey);
            id = $"{reqBody.parentId}-{counter}"; // session id already included in parent id
        }

        NodeInfo nodeinfo = new NodeInfo
        {
            Id = id,
            Pid = reqBody.parentId,
            Ready = "false",
            Payload = reqBody.payload,
            SessionId = reqBody.SessionId
        };

        // The following creates the ndoe process instance on the cluter.
        try
        {
            UpAndDown.Up(nodeinfo);
        }
        catch (Exception ex)
        {
            return (500, new JsonObject
            {
                ["error"] = ex.Message
            });
        }

        // Success :)
        return (200, new JsonObject
        {
            ["message"] = "Child node scheduled for creation."
        });
    }

    #endregion

}