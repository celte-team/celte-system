using Microsoft.VisualBasic;
using StackExchange.Redis;

class RedisDb
{


    private static Lazy<ConnectionMultiplexer> lazyConnection = new Lazy<ConnectionMultiplexer>(() =>
    {
        string celteRedisHost = Environment.GetEnvironmentVariable("CELTE_REDIS_HOST") ?? string.Empty;
        var config = ConfigurationOptions.Parse(celteRedisHost + ":6379");
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

    public static string GetSNFromSpawnerId(string spawnerId)
    {
        string sessionId = Utils.GetConfigOption("sessionId", "default");
        string redisKey = sessionId + spawnerId;
        string? value = GetString(redisKey);
        if (value == null)
        {
            throw new Exception($"SN not found for spawnerId: {spawnerId}");
        }
        return value;
    }
}