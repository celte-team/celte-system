// using System;
// using System.Threading.Tasks;
// using Docker.DotNet;
// using Docker.DotNet.Models;
// public class ContainerManager
// {
//     // the goal of this class is to manage docker containers

//     public ContainerManager()
//     {
//         // constructor
//     }

//     ~ContainerManager()
//     {
//         // destructor
//     }
//     // this method will start a container
//     public async void StartContainer(string containerName)
//     {
// // Initialize the DockerClient
//         // DockerClientConfiguration config = new DockerClientConfiguration(new Uri("unix:///var/run/docker.sock"));
//         // DockerClient client = config.CreateClient();
//         DockerClient client = new DockerClientConfiguration().CreateClient();
//         Console.WriteLine("Client created!!!!!!!!!!!!1");
//         // Create the container
//         CreateContainerResponse response = await client.Containers.CreateContainerAsync(new CreateContainerParameters
//         {
//             Image = "clmt/celte-serverstack", // Replace with your image name
//             Name = "blop", // Replace with your container name
//             Tty = true,
//             HostConfig = new HostConfig
//             {
//                 PortBindings = new Dictionary<string, IList<PortBinding>>
//                 {
//                     { "80/tcp", new List<PortBinding> { new PortBinding { HostPort = "8082" } } }
//                 }
//             }
//         });

//         // Start the container
//         if (response.ID != null)
//         {
//             bool started = await client.Containers.StartContainerAsync(response.ID, new ContainerStartParameters());
//             Console.WriteLine(started ? "Container started successfully" : "Failed to start container");
//         }
//         else
//         {
//             Console.WriteLine("Failed to create container");
//         }
//     }

//     // this method will stop a container
//     public void StopContainer(string containerName)
//     {
//         // stop the container
//     }

//     // this method will restart a container
//     public void RestartContainer(string containerName)
//     {
//         // stop the container
//         // start the container
//     }
// }