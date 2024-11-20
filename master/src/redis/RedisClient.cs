using System;
using System.Threading.Tasks;
using NRedisStack;
using NRedisStack.RedisStackCommands;
using StackExchange.Redis;
using Newtonsoft.Json;


namespace Redis {
    /// <summary>
    /// Represents a log entry
    /// </summary>
    public class ActionLog
    {
        public string ActionType { get; set; } = string.Empty;
        public DateTime Timestamp { get; set; } = DateTime.Now;
        public string Details { get; set; } = string.Empty;
    }

    /// <summary>
    /// Singleton Redis client for managing Redis connections
    /// </summary>
    partial class RedisClient
    {
        private static RedisClient _instance;
        private readonly ConnectionMultiplexer _connection;
        private readonly IDatabase _db;
        public RedisData redisData;
        public RLogger rLogger;

        private RedisClient(string connectionString)
        {
            try {
                _connection = ConnectionMultiplexer.Connect(connectionString);
                _db = _connection.GetDatabase();
                Console.WriteLine("Connected to Redis");
            } catch (Exception ex) {
                Console.WriteLine($"Error connecting to Redis: {ex.Message}");
            }
        }

        public static RedisClient GetInstance(string connectionString = null)
        {
            if (_instance == null)
            {

                _instance = new RedisClient(connectionString);
                RedisData Rd = new RedisData(_instance.GetDatabase());
                _instance.redisData = Rd;
                RLogger Rl = new RLogger(_instance.GetDatabase());
                _instance.rLogger = Rl;
            }
            return _instance;
        }

        public IDatabase GetDatabase() => _db;

        public void Dispose()
        {
            _connection?.Dispose();
        }
    }

    /// <summary>
    /// Serializes and deserializes JSON objects to/from strings
    /// </summary>
    public static class JSONSerializer
    {
        public static string Serialize(object obj)
        {
            return JsonConvert.SerializeObject(obj);
        }

        public static T Deserialize<T>(string json)
        {
            return JsonConvert.DeserializeObject<T>(json);
        }
    }

    /// <summary>
    /// Sends log actions to Redis
    /// </summary>

    public class RLogger
    {
        private readonly IDatabase _db;

        public RLogger(IDatabase db)
        {
            _db = db;
        }

        public async Task LogActionAsync(ActionLog log)
        {
            string logJson = JSONSerializer.Serialize(log);
            string key = "action_logs";

            if (_db == null)
            {
                Console.WriteLine("Database is null");
                throw new Exception("Database is null");
            }

            try
            {
                // Ensure the key exists as a JSON array
                if (!await _db.KeyExistsAsync(key))
                {
                    await _db.ExecuteAsync("JSON.SET", key, "$", "[]");
                }

                // Append the new log entry to the JSON array
                await _db.ExecuteAsync("JSON.ARRAPPEND", key, "$", logJson);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error logging action: {ex.Message}");
            }
        }
    }

    /// <summary>
    /// General Redis data operations
    /// </summary>
    public class RedisData
    {
        private readonly IDatabase _db;

        public RedisData(IDatabase db)
        {
            _db = db;
        }

        public void SetValue(string key, string value)
        {
            _db.StringSet(key, value);
        }

        public void SetValue(string key, int value)
        {
            _db.StringSet(key, value);
        }

        public void SetValue(string key, object value)
        {
            _db.StringSet(key, JSONSerializer.Serialize(value));
        }

        public string GetValue(string key)
        {
            return _db.StringGet(key);
        }

        public void IncrementValue(string key)
        {
            _db.StringIncrement(key);
        }

        public async Task<bool> JSONPush(string key, string field, object value)
        {
            if (!_db.KeyExists(key))
            {
                _db.Execute("JSON.SET", key, "$", "[]");
            }

            string jsonValue = JSONSerializer.Serialize(value);
            Console.WriteLine("key: " + key + " field: " + field + " jsonValue: " + jsonValue);
            await _db.ExecuteAsync("JSON.ARRAPPEND", key, $"$.{field}", jsonValue);
            return true;
        }

        public async Task<bool> JSONPush(string key, string field, string value)
        {
            try {
                if (!await _db.KeyExistsAsync(key))
                {
                    _db.Execute("JSON.SET", key, "$", "[]");
                }
                // send the value to the key
                await _db.ExecuteAsync("JSON.ARRAPPEND", key, "$",
                    JSONSerializer.Serialize(value));

                await _db.ExecuteAsync("JSON.ARRAPPEND", "action_logs", "$",
                    JSONSerializer.Serialize(new ActionLog
                    {
                        ActionType = "JSONPush",
                        Timestamp = DateTime.Now,
                        Details = $"Pushed value to key {key}"
                    }));

            } catch (Exception ex) {
                Console.WriteLine($"Error pushing JSON value: {ex.Message}");
            }
            return true;
        }

        public async Task<T> JSONGetAll<T>(string key)
        {
            try
            {
                var jsonValue = await _db.ExecuteAsync("JSON.GET", key);
                T value = JSONSerializer.Deserialize<T>(jsonValue.ToString());
                return value;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error getting JSON value: {ex.Message}");
                return default(T);
            }
        }

        public async Task<string> JSONGet(string key, string field)
        {
            try
            {
                var jsonValue = await _db.ExecuteAsync("JSON.GET", key, $"$.{field}");
                string value = jsonValue.ToString();
                return value;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error getting JSON value: {ex.Message}");
                return null;
            }
        }

        public async Task<string> JSONGetAll(string key)
        {
            try
            {
                var jsonValue = await _db.ExecuteAsync("JSON.GET", key);
                string value = jsonValue.ToString();
                return value;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error getting JSON value: {ex.Message}");
                return null;
            }
        }

        public void JSONRemove(string key, string field)
        {
            _db.Execute("JSON.DEL", key, $"$.{field}");
        }

        // JSON-specific operations
        public async Task JSONSetValueAsync(string key, string field, object value)
        {
            try
            {
                // Ensure the key is created as a JSON object
                if (!await _db.KeyExistsAsync(key))
                {
                    await _db.ExecuteAsync("JSON.SET", key, "$", "{}");
                }

                string jsonValue = JSONSerializer.Serialize(value);
                await _db.ExecuteAsync("JSON.SET", key, $"$.{field}", jsonValue);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error setting JSON value: {ex.Message}");
            }
        }


        /// <summary>
        /// Check if a JSON field exists
        /// </summary>
        /// <param name="key"></param>
        /// <param name="field"></param>
        /// <returns></returns>
        public bool JSONExists(string key, string field)
        {
            try
            {
                var jsonValue = _db.ExecuteAsync("JSON.GET", key, $"$.{field}");
                if (jsonValue == null)
                {
                    return false;
                }
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error checking JSON value: {ex.Message}");
                return false;
            }
        }
    }
}