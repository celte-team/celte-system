
/// <summary>
/// The goal of this class is to manage the Docker system.
/// </summary>
///
using Docker.DotNet;
using Docker.DotNet.Models;
class DockerSystem
{
    private readonly DockerClient _client;
    private List<string> _containerIds = new List<string>();
    private Dictionary<string, object>? _yamlObject;

    public DockerSystem(Dictionary<string, object>? yamlObject)
    {
        _yamlObject = yamlObject;
        _client = new DockerClientConfiguration().CreateClient();
    }

    ~DockerSystem()
    {
        Dispose();
    }

    public async Task LaunchContainer()
    {
        try
        {
            if (_yamlObject?["container_image"] == null)
            {
                Console.WriteLine("No 'container_image' key found in the configuration file.");
                return;
            }

            var UUID = "celte" + Guid.NewGuid().ToString();
            var response = await _client.Containers.CreateContainerAsync(new CreateContainerParameters
            {
                Name = UUID,
                Image = _yamlObject["container_image"].ToString(),
                HostConfig = new HostConfig()
                {
                    NanoCPUs = _yamlObject.ContainsKey("cpu") ? Convert.ToInt64(_yamlObject["cpu"]) : 100000000L,
                    Memory = _yamlObject.ContainsKey("memory") ? Convert.ToInt64(_yamlObject["memory"]) * 1024 * 1024 : 512 * 1024 * 1024
                }
            });

            _containerIds.Add(response.ID);

            // Start the container
            bool started = await _client.Containers.StartContainerAsync(response.ID, new ContainerStartParameters());
            if (!started)
            {
                Console.WriteLine($"Failed to start container {UUID}.");
            }
            else
            {
                Console.WriteLine($"Container {UUID} started successfully.");
            }
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error launching container: {e.Message}");
            Environment.Exit(1);
        }
    }

    public async Task ShutdownContainer()
    {
        if (_containerIds.Count > 0)
        {
            foreach (var containerIds in _containerIds)
            {
                // delete the container
                _client.Containers.RemoveContainerAsync(
                    containerIds,
                    new ContainerRemoveParameters
                    {
                        Force = true
                    });
                Console.WriteLine($"Container {containerIds} deleted.");
            }
        }
    }

    public void Dispose()
    {
        _client?.Dispose();
    }
}