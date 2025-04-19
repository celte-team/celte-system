using System.Text.Json;
using TextCopy;

class UpAndDown
{
    public static void Up(Nodes.NodeInfo nodeinfo)
    {
        // todo : kubernetes goes here @clement
        string command = $"export CELTE_MODE=server; export CELTE_NODE_ID={nodeinfo.Id}; export CELTE_NODE_PID={nodeinfo.Pid}; godot . --headless";
        ClipboardService.SetText(command);
        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine("Command to create the node has been copied to clipboard");
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine("In development mode, use the command that has been copied to your clipboard to create the node manually by running it in the folder of the project of the node.");
        Console.WriteLine($"\t{command}");
        Console.ResetColor();
        RedisDb.SetHashField("nodes", nodeinfo.Id, JsonSerializer.Serialize(nodeinfo));
    }
}