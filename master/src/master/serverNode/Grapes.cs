using System.ComponentModel;
using System.Text.Json;
class Grape
{

    Master _master = Master.GetInstance();

    public Grape()
    {
    }

    /// <summary>
    /// Get all grapes from the configuration
    /// </summary>
    /// <returns></returns>
    /// <exception cref="InvalidOperationException"></exception>
    public List<string> getGrapesStr()
    {
        if (_master._setupConfig == null)
        {
            throw new InvalidOperationException("Setup configuration is not initialized.");
        }
        Dictionary<string, object>? yamlObject = _master._setupConfig.GetYamlObjectConfig();
        var grapes = JsonSerializer.Serialize(yamlObject?["grapes"]);
        List<string> grapesStr = JsonSerializer.Deserialize<List<string>>(grapes);
        if (grapes == null || grapesStr.Count == 0)
        {
            throw new InvalidOperationException("No grapes found in the configuration.");
        }
        return grapesStr;
    }

    public async Task<string> ReturnNextGrape()
    {
        List<string> grapes = getGrapesStr();
        List<string> assignedGrapes = await ReturnAllAssignedGrapes();
        foreach (string grape in grapes)
        {
            if (!assignedGrapes.Contains(grape))
            {
                return grape;
            }
        }
        return null;
    }

    public async Task<List<string>> ReturnAllAssignedGrapes()
    {
        List<string> grapes = new List<string>();
        JsonElement nodesJson = await Redis.RedisClient.GetInstance().redisData.JSONGetAll("nodes");
        if (nodesJson.ValueKind != JsonValueKind.Array)
        {
            return grapes;
        }
        foreach (JsonElement node in nodesJson.EnumerateArray())
        {
            using (JsonDocument nodeDoc = JsonDocument.Parse(node.GetString()))
            {
                JsonElement root = nodeDoc.RootElement;
                if (root.TryGetProperty("grapes", out JsonElement grapesProperty) &&
                    grapesProperty.ValueKind == JsonValueKind.Array)
                {
                    foreach (JsonElement grape in grapesProperty.EnumerateArray())
                    {
                        if (grape.ValueKind == JsonValueKind.String)
                        {
                            grapes.Add(grape.GetString());
                        }
                    }
                }
            }
        }
        Console.WriteLine($"<Grape> Found assigned grapes: {string.Join(", ", grapes)}");
        return grapes;
    }


    /// <summary>
    /// Use to get the grape from the node id, to spawn a player in the good SN
    /// </summary>
    /// <param name="nodeId"></param>
    /// <returns></returns>
    public async Task<string> GetGrapeFromNodeId(string nodeId)
    {
        JsonElement nodesJson = await Redis.RedisClient.GetInstance().redisData.JSONGetAll("nodes");
        string grapeName = "";
        foreach (JsonElement node in nodesJson.EnumerateArray())
        {
            using (JsonDocument nodeDoc = JsonDocument.Parse(node.GetString()))
            {
                JsonElement root = nodeDoc.RootElement;
                if (root.TryGetProperty("uuid", out JsonElement uuidProperty) &&
                    uuidProperty.ValueKind == JsonValueKind.String)
                {
                    string uuid = uuidProperty.GetString();
                    if (uuid == nodeId)
                    {
                        foreach (JsonElement grape in root.GetProperty("grapes").EnumerateArray())
                        {
                            if (grape.ValueKind == JsonValueKind.String)
                            {
                                grapeName = grape.GetString();
                                Console.WriteLine($"<GrapeName> Found grape: {grapeName}");
                                return grapeName;
                            }
                        }
                    }
                }
            }
        }
        return null;
    }
}