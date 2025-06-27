
using YamlDotNet.RepresentationModel;

class Utils
{
    /// <summary>
    /// Retries an action a specified number of times with a delay between attempts.
    /// Attempts are considered failed if an exception is thrown.
    /// </summary>
    /// <param name="action"></param>
    /// <param name="maxRetries"></param>
    /// <param name="delay"></param>

    private static Dictionary<string, string> masterConfig;

    public static void LoadYamlConfig(string yamlFilePath)
    {
        if (!File.Exists(yamlFilePath))
        {
            throw new FileNotFoundException("YAML configuration file not found " + yamlFilePath, yamlFilePath);
        }

        var yaml = new YamlStream();
        using (var reader = new StreamReader(yamlFilePath))
        {
            yaml.Load(reader);
        }

        var root = (YamlMappingNode)yaml.Documents[0].RootNode;

        if (!root.Children.TryGetValue(new YamlScalarNode("celte"), out YamlNode celteNode))
        {
            throw new Exception("Missing 'celte' section in YAML file");
        }

        var dict = new Dictionary<string, string>();
        foreach (var entry in (YamlSequenceNode)celteNode)
        {
            if (entry is YamlMappingNode map)
            {
                foreach (var kv in map.Children)
                {
                    string key = ((YamlScalarNode)kv.Key).Value;
                    string value = ((YamlScalarNode)kv.Value).Value;
                    dict[key] = value;
                }
            }
        }

        masterConfig = dict;
    }

    public static T Retry<T>(Func<T> action, int maxRetries = 3, int delay = 100)
    {
        int attempts = 0;
        while (attempts < maxRetries)
        {
            try
            {
                T result = action();
                return result;
            }
            catch (RetryableException)
            {
                attempts++;
                if (attempts >= maxRetries)
                {
                    throw;
                }
                System.Threading.Thread.Sleep(delay);
            }
            catch (Exception ex)
            {
                throw new Exception($"An error occurred: {ex.Message}", ex);
            }
        }
        throw new InvalidOperationException("Retry failed to return a result.");
    }

    /// <summary>
    /// Logs a message to Redis under the 'master_logs' key.
    /// </summary>
    /// <param name="message"></param>
    public static void LogToRedis(string message)
    {
        RedisDb.SetString("master_logs", $"{DateTime.UtcNow}: {message}");
    }

    public static string GetConfigOption(string key, string defaultValue)
    {
        if (masterConfig == null)
        {
            throw new InvalidOperationException("YAML config not loaded. Call LoadYamlConfig() first.");
        }

        return masterConfig.TryGetValue(key, out var value) ? value : defaultValue;
    }
}
