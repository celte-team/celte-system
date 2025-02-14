using System;
using System.IO;
using System.Linq;
using YamlDotNet.Serialization;
using YamlDotNet.Serialization.NamingConventions;
using System.Collections.Generic;
using System.Threading.Tasks;

class SetupConfig
{
    private readonly string[] _args;
    private Dictionary<string, object>? _yamlObject;
    protected DockerSystem? dockerSystem;
    public List<string> _grapes = new List<string>();
    public SetupConfig(string[] args)
    {
        _args = args;
    }

    ~SetupConfig()
    {
    }

    public Dictionary<string, object>? GetYamlObjectConfig()
    {
        if (_yamlObject == null || _yamlObject.Count == 0)
        {
            Console.WriteLine("Yaml object is null.");
            return null;
        }
        return _yamlObject;
    }

    public void SettingUpMaster()
    {
        try
        {
            GetConfigFile();
            GetNumberOfGrapes();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Master: {e.Message}");
        }

    }

    private int GetNumberOfGrapes()
    {
        // Console.WriteLine($"Getting number of grapes...: {_yamlObject["grapes"]}");
        if (_yamlObject != null && _yamlObject.ContainsKey("grapes"))
        {
            var grapes = _yamlObject["grapes"] as List<object>;
            if (grapes != null)
            {
                _grapes = grapes.Select(g => g.ToString()).ToList();
                Console.WriteLine($"Number of grapes: {grapes.Count}");
                return grapes.Count;
            }
        }

        Console.WriteLine("No 'grapes' key found in the configuration file or it is not a list.");
        return 0;
    }



    public async Task Shutdown()
    {
        if (dockerSystem != null)
            await dockerSystem.ShutdownContainer();
    }

    private void ReadConfigFile(string configFilePath)
    {
        try
        {
            if (File.Exists(configFilePath))
            {
                string fileContents = File.ReadAllText(configFilePath);

                var deserializer = new DeserializerBuilder()
                    .WithNamingConvention(CamelCaseNamingConvention.Instance)
                    .Build();

                _yamlObject = deserializer.Deserialize<Dictionary<string, object>>(fileContents);
                dockerSystem = new DockerSystem(_yamlObject);
            }
            else
            {
                Console.WriteLine($"The file '{configFilePath}' does not exist.");
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"An error occurred while trying to read the file: {ex.Message}");
        }
    }

    private void GetConfigFile()
    {
        string? configFilePath = null;

        for (int i = 0; i < _args.Length; i++)
        {
            if ((_args[i] == "--config" || _args[i] == "-f") && i + 1 < _args.Length)
            {
                configFilePath = _args[i + 1];
                break;
            }
        }

        if (configFilePath != null)
        {
            ReadConfigFile(configFilePath);
        }
        else
        {
            string? configFileEnv = Environment.GetEnvironmentVariable("CONFIG_FILE");
            if (configFileEnv != null)
            {
                ReadConfigFile(configFileEnv);
            }
            else
            {
                Console.WriteLine("No configuration file provided. Either use the --config option or set the CONFIG_FILE environment variable.");

            }
        }
    }
}
