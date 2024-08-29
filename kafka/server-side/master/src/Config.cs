using System;
using System.IO;
using YamlDotNet.RepresentationModel;

public class Config
{
    public int Port = 11000;
    public string IP = "0.0.0.0";

    public string RLIP = "";
    public int RLPort;

    public bool GetAllArgs(string[] args)
    {
        try
        {
            string? portArg = GetArgumentValue(args, "--port");
            if (portArg != null)
                Port = int.Parse(portArg);

            string? configArg = GetArgumentValue(args, "--config");
            if (configArg == null)
            {
                // help:
                Console.WriteLine("Usage: dotnet run --port ... --config <config_file_path>");
                return false;
            }
            var configFileContent = File.ReadAllText(configArg);
            ParseConfigFile(configFileContent);
        }
        catch
        {
            return false;
        }
        return true;
    }

    private string? GetArgumentValue(string[] args, string key)
    {
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == key && i + 1 < args.Length)
            {
                return args[i + 1];
            }
        }
        return null;
    }

    private void ParseConfigFile(string content)
    {
        Console.WriteLine("yaml.Documents[0].RootNode.NodeType");
        var input = new StringReader(content);
        var yaml = new YamlStream();
        yaml.Load(input);
        var mapping = (YamlMappingNode)yaml.Documents[0].RootNode;
        var configuration = (YamlMappingNode)mapping.Children[new YamlScalarNode("configuration")];
        var relationLayerConfig = (YamlMappingNode)configuration.Children[new YamlScalarNode("relation_layer_configuration")];
        RLIP = relationLayerConfig.Children[new YamlScalarNode("ip")].ToString();
        RLPort = int.Parse(relationLayerConfig.Children[new YamlScalarNode("port")].ToString());
    }
}
