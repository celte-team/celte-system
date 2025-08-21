using System.Text.Json;
using System.Diagnostics;
using System.Collections.Concurrent;

class UpAndDown
{
    private static readonly ConcurrentDictionary<string, Process> _processes = new();

    public static void Up(Nodes.NodeInfo nodeinfo)
    {
        // get godot_path from the environment variables
        string celte_godot_project_path = Utils.GetConfigOption("CELTE_GODOT_PROJECT_PATH", string.Empty);
        string godot_path = Utils.GetConfigOption("CELTE_GODOT_PATH", string.Empty);


        if (string.IsNullOrEmpty(celte_godot_project_path) || string.IsNullOrEmpty(godot_path))
        {
            throw new InvalidOperationException("CELTE_GODOT_PROJECT_PATH or CELTE_GODOT_PATH is not set");
        }

        // Create logs directory if it doesn't exist
        string logsDir = Path.Combine(celte_godot_project_path, "logs");
        Directory.CreateDirectory(logsDir);
        string logFile = Path.Combine(logsDir, $"{nodeinfo.Id}.log");
        if (File.Exists(logFile))
        {
            File.Delete(logFile);
        }

        // Prepare the base command (without local log redirection)
        string headlessMode = "";
        if (Utils.GetConfigOption("CELTE_SERVER_GRAPHICAL_MODE", "false") != "true")
        {
            headlessMode = "--headless";
        }
        string baseCommand = $"cd {celte_godot_project_path} ; export CELTE_MODE=server; export CELTE_NODE_ID={nodeinfo.Id}; export CELTE_NODE_PID={nodeinfo.Pid}; {godot_path} . {headlessMode}";

        // If running inside a container, run a sibling container with the command provided
        if (IsRunningInContainer())
        {
            string dockerCmd = $"docker run -d clmt/celte-sn /bin/sh -lc \"{baseCommand}\"";
            var startInfoContainer = new ProcessStartInfo
            {
                FileName = "/bin/sh",
                Arguments = $"-c \"{dockerCmd}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };
            var procContainer = new Process { StartInfo = startInfoContainer };
            procContainer.Start();
            string containerId = procContainer.StandardOutput.ReadToEnd().Trim();
            Console.WriteLine(string.IsNullOrEmpty(containerId)
                ? $"Failed to start node container: {procContainer.StandardError.ReadToEnd()}"
                : $"Started node container: {containerId}");
            return;
        }

        // Append local logging for non-container runs
        string command = baseCommand + $" > {logFile} 2>&1";
        Console.WriteLine($"Starting node {nodeinfo.Id} with command: {command}");

        if (OperatingSystem.IsMacOS())
        {
            // Start the process in the background
            var startInfo = new ProcessStartInfo
            {
                FileName = "/bin/bash",
                Arguments = $"-c \"{command}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            var process = new Process { StartInfo = startInfo };
            process.Start();

            // Store the process for later cleanup
            _processes.TryAdd(nodeinfo.Id, process);

            // Store the process ID in Redis for later reference
            nodeinfo.Pid = process.Id.ToString();


            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"Node {nodeinfo.Id} started with PID {process.Id}");
            Console.WriteLine($"Logs are being written to: {logFile}");
            Console.ResetColor();
            // }

            RedisDb.SetHashField("nodes", nodeinfo.Id, JsonSerializer.Serialize(nodeinfo));

        }
        else if (OperatingSystem.IsWindows())
        {
            // Windows implementation
            var startInfo = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c {command}",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            var process = new Process { StartInfo = startInfo };
            process.Start();

            // Store the process for later cleanup
            _processes.TryAdd(nodeinfo.Id, process);

            // Store the process ID in Redis for later reference
            nodeinfo.Pid = process.Id.ToString();
            RedisDb.SetHashField("nodes", nodeinfo.Id, JsonSerializer.Serialize(nodeinfo));

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"Node {nodeinfo.Id} started with PID {process.Id}");
            Console.WriteLine($"Logs are being written to: {logFile}");
            Console.ResetColor();
        }
        else if (OperatingSystem.IsLinux())
        {
            // Linux implementation
            var startInfo = new ProcessStartInfo
            {
                FileName = "/bin/bash",
                Arguments = $"-c \"{command}\"",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            var process = new Process { StartInfo = startInfo };
            process.Start();

            // Store the process for later cleanup
            _processes.TryAdd(nodeinfo.Id, process);

            // Store the process ID in Redis for later reference
            nodeinfo.Pid = process.Id.ToString();
            RedisDb.SetHashField("nodes", nodeinfo.Id, JsonSerializer.Serialize(nodeinfo));

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"Node {nodeinfo.Id} started with PID {process.Id}");
            Console.WriteLine($"Logs are being written to: {logFile}");
            Console.ResetColor();
        }
        else
        {
            throw new NotSupportedException("Unsupported operating system");
        }
    }

    public static void Down(Nodes.NodeInfo nodeinfo)
    {
        if (_processes.TryRemove(nodeinfo.Id, out Process? process))
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill(true); // Kill the process and its children
                    process.WaitForExit(5000); // Wait up to 5 seconds for the process to exit
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error killing process {nodeinfo.Id}: {ex.Message}");
            }
        }
    }

    public static void CleanupAllProcesses()
    {
        foreach (var process in _processes.Values)
        {
            try
            {
                if (!process.HasExited)
                {
                    process.Kill(true);
                    process.WaitForExit(5000);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error killing process {process.Id}: {ex.Message}");
            }
        }
        _processes.Clear();
    }

    private static bool IsRunningInContainer()
    {
        try
        {
            // Common indicators for containerized environments
            if (File.Exists("/.dockerenv")) return true;
            var inContainer = Environment.GetEnvironmentVariable("DOTNET_RUNNING_IN_CONTAINER");
            if (!string.IsNullOrEmpty(inContainer) && inContainer.ToLowerInvariant() == "true") return true;
            // Fallback: cgroup check
            if (File.Exists("/proc/1/cgroup"))
            {
                var text = File.ReadAllText("/proc/1/cgroup");
                if (text.Contains("docker") || text.Contains("kubepods")) return true;
            }
        }
        catch { }
        return false;
    }

    private static string EnvForward(string name)
    {
        var value = Environment.GetEnvironmentVariable(name);
        return string.IsNullOrEmpty(value) ? string.Empty : $"-e {name}={value} ";
    }
}
