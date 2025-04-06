using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;

namespace Celte.Master.K8s
{
    public class SetupConfig
    {
        private readonly string _configPath;
        private Dictionary<string, object> _config;

        public SetupConfig(string[] args)
        {
            _configPath = args.Length > 0 ? args[0] : "/config/configFile.yml";
            _config = new Dictionary<string, object>();
        }

        public bool IsProduction()
        {
            try
            {
                var config = GetYamlObjectConfig();
                if (config.ContainsKey("production"))
                {
                    return config["production"].ToString().ToLower() == "true";
                }
                Console.WriteLine("Production flag not found in config, defaulting to false");
                return false;
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error checking production status: {e.Message}");
                return false;
            }
        }

        public Dictionary<string, object> GetYamlObjectConfig()
        {
            if (_config.Count > 0)
            {
                return _config;
            }

            try
            {
                string yamlContent = File.ReadAllText(_configPath);
                Console.WriteLine($"Reading config from: {_configPath}");
                Console.WriteLine($"Config content: {yamlContent}");

                var deserializer = new DeserializerBuilder()
                    .WithNamingConvention(CamelCaseNamingConvention.Instance)
                    .Build();

                var config = deserializer.Deserialize<Dictionary<string, object>>(yamlContent);

                if (config == null)
                {
                    throw new Exception("Config file is empty or invalid");
                }

                // Only validate required fields if production is true
                if (config.ContainsKey("production") && config["production"].ToString().ToLower() == "true")
                {
                    var requiredFields = new[] { "namespace", "deploymentName", "maxReplicas", "minReplicas" };
                    foreach (var field in requiredFields)
                    {
                        if (!config.ContainsKey(field))
                        {
                            throw new Exception($"Required field '{field}' is missing in config");
                        }
                    }
                }

                _config = config;
                return _config;
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error reading config file: {e.Message}\n{e.StackTrace}");
                // Return default values as fallback
                _config = new Dictionary<string, object>
                {
                    { "production", false },
                    { "namespace", "celte" },
                    { "deploymentName", "server-node" },
                    { "maxReplicas", 5 },
                    { "minReplicas", 1 }
                };
                return _config;
            }
        }
    }
}