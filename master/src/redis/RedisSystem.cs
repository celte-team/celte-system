using System;
using System.Threading.Tasks;
using NRedisStack;
using NRedisStack.RedisStackCommands;
using StackExchange.Redis;

class RedisSystem {
    string password;
    IDatabase db;
    public RedisSystem(string password) {
        this.password = password;
        ConnectToRedis().GetAwaiter().GetResult();
    }

    public async Task ConnectToRedis() {
        Console.WriteLine("Connecting to Redis...");

        var options = new ConfigurationOptions {
            EndPoints = { "localhost:6379" },
            Password = password,
            Ssl = false
        };

// code /opt/homebrew/etc/redis.conf
        try {
            var _redis = await ConnectionMultiplexer.ConnectAsync(options);
            db = _redis.GetDatabase();
            Console.WriteLine("Connected to Redis!");
            await db.StringSetAsync("myKey", "myValue12312312312");
            Console.WriteLine("Set value for 'myKey'");

            var value = await db.StringGetAsync("myKey");
            Console.WriteLine($"Value for 'myKey': {value}");

            // while (true) {
            //     Console.WriteLine("Ping Redis!: " + db.Ping());
            //     await Task.Delay(1000); // Using Task.Delay to avoid blocking the thread
            // }
        } catch (Exception e) {
            Console.WriteLine("Error connecting to Redis: " + e.Message);
        }
    }

    public void SetValue(string key, string value) {
        db.StringSet(key, value);
    }

    public void SetValue(string key, int value) {
        db.StringSet(key, value);
    }
    public void SetValue(string key, object value) {
        db.StringSet(key, value.ToString());
    }

    public void IncrementValue(string key) {
        db.StringIncrement(key);
    }

    public string GetValue(string key) {
        return db.StringGet(key);
    }
}
