using Microsoft.VisualBasic;
using StackExchange.Redis;

class RedisDb
{


    private static Lazy<ConnectionMultiplexer> lazyConnection = new Lazy<ConnectionMultiplexer>(() =>
    {
        string celteRedisHost = Utils.GetConfigOption("CELTE_REDIS_HOST", "localhost");
        string celteRedisPort = Utils.GetConfigOption("CELTE_REDIS_PORT", "6379");
        var config = ConfigurationOptions.Parse($"{celteRedisHost}:{celteRedisPort}");
        Console.WriteLine($"Connecting to Redis at {celteRedisHost}:{celteRedisPort}");
        config.ConnectRetry = 5;
        config.AbortOnConnectFail = false;
        return ConnectionMultiplexer.Connect(config);
    });

    public static ConnectionMultiplexer Connection => lazyConnection.Value;

    public static IDatabase Database => Connection.GetDatabase();

    public static void SetString(string key, string value)
    {
        Database.StringSet(key, value);
    }

    public static string? GetString(string key)
    {
        return Database?.StringGet(key);
    }

    public static void SetHashField(string hashKey, string fieldKey, string value)
    {
        Database.HashSet(hashKey, fieldKey, value);
    }

    public static string? GetHashField(string hashKey, string fieldKey)
    {
        return Database.HashGet(hashKey, fieldKey);
    }

    public static string GetSNFromSpawnerId(string spawnerId, string sessionId)
    {
        string redisKey = sessionId + spawnerId;
        string? value = GetString(redisKey);
        Console.WriteLine($"Retrieved SN for redisKey: {redisKey} - {value}");
        if (value == null)
        {
            throw new Exception($"SN not found for spawnerId: {spawnerId}");
        }
        return value;
    }

    // Clear all keys belonging to a session. Project convention: keys start with the sessionId.
    // This method deletes keys that begin with the sessionId (e.g. "<sessionId>..."),
    // supports the future canonical prefix "session:{sessionId}:*" as well, and also
    // removes hash fields under known hashes (currently 'nodes') whose field names
    // begin with the sessionId.
    public static void ClearSession(string sessionId)
    {
        if (string.IsNullOrEmpty(sessionId))
            throw new ArgumentException("sessionId is required", nameof(sessionId));

        var endpoints = Connection.GetEndPoints();
        foreach (var endpoint in endpoints)
        {
            var server = Connection.GetServer(endpoint);
            var patterns = new[] { $"{sessionId}*", $"session:{sessionId}:*" };
            foreach (var pat in patterns)
            {
                // Iterate with server-side scanning
                foreach (var key in server.Keys(pattern: pat))
                {
                    try { Database.KeyDelete(key); } catch { }
                }
            }
        }

        // Clean up 'nodes' hash fields where the field name (node id) starts with sessionId
        try
        {
            var entries = Database.HashGetAll("nodes");
            foreach (var he in entries)
            {
                var field = he.Name.ToString();
                if (!string.IsNullOrEmpty(field) && field.StartsWith(sessionId, StringComparison.Ordinal))
                {
                    try { Database.HashDelete("nodes", he.Name); } catch { }
                }
            }
        }
        catch { }
    }
}