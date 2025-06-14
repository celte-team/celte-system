using System.Text.Json;
using System.Diagnostics;
using System.Collections.Concurrent;

class UpAndDown
{
    private static readonly ConcurrentDictionary<string, Process> _processes = new();

    public static void Up(Nodes.NodeInfo nodeinfo)
    {
        // get godot_path from the environment variables
        string celte_godot_project_path = Environment.GetEnvironmentVariable("CELTE_GODOT_PROJECT_PATH") ?? throw new InvalidOperationException("CELTE_GODOT_PROJECT_PATH is not set");
        string godot_path = Environment.GetEnvironmentVariable("CELTE_GODOT_PATH") ?? throw new InvalidOperationException("CELTE_GODOT_PATH is not set");

        // Create logs directory if it doesn't exist
        string logsDir = Path.Combine(celte_godot_project_path, "logs");
        Directory.CreateDirectory(logsDir);
        string logFile = Path.Combine(logsDir, $"{nodeinfo.Id}.log");
        if (File.Exists(logFile))
        {
            File.Delete(logFile);
        }

        // Prepare the command
        string headlessMode = "";
        if (Environment.GetEnvironmentVariable("CELTE_SERVER_GRAPHICAL_MODE") != "true")
        {
            headlessMode = "--headless"; ;
        }
        string command = $"cd {celte_godot_project_path} ; export CELTE_MODE=server; export CELTE_NODE_ID={nodeinfo.Id}; export CELTE_NODE_PID={nodeinfo.Pid}; DYLD_INSERT_LIBRARIES=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/17/lib/darwin/libclang_rt.asan_osx_dynamic.dylib {godot_path} . {headlessMode}> {logFile} 2>&1";
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

            // if (Environment.GetEnvironmentVariable("CELTE_SERVERS_MANUAL") != "true")
            // {
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
}