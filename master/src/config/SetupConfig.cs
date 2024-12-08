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
        GetConfigFile();
        GetNumberOfGrapes();
        // if (_args.Contains("--local") || _args.Contains("-l"))
        // {
        //     SettingUpLocal();
        // }
        // else if (_args.Contains("--cloud") || _args.Contains("-c"))
        // {
        //     // TODO Implement cloud setup
        //     // SettingUpCloud();
        // }
        // else
        // {
        //     // for the moment I allow the non usage of the flags for testing purposes

        //     // Usage usage = new Usage();
        //     // usage.UsageMessage();
        //     // return;
        // }
    }

    public void SettingUpLocal()
    {
        if (_yamlObject != null)
        {
            int nGrapes = _grapes.Count;
            Console.WriteLine($"Launching {nGrapes} containers...");

            for (int i = 0; i < nGrapes; i++)
            {
                dockerSystem.LaunchContainer().Wait();
            }
        }
        else
        {
            Console.WriteLine("Failed to load the configuration file.");
        }
    }

    private int GetNumberOfGrapes()
    {
        if (_yamlObject != null && _yamlObject.ContainsKey("grapes"))
        {
            var grapes = _yamlObject["grapes"] as List<object>;
            if (grapes != null)
            {
                _grapes = grapes.Select(g => g.ToString()).ToList();
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
        else
        {
            Console.WriteLine("Usage: --config <configFile.yml>");
        }
    }
}
