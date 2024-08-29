public class NodeHandler : AHandler
{
    public static List<NodeHandler> Instances = new List<NodeHandler>();
    public NodeHandler(string ip, int port) : base(ip, port)
    {
        _ip = ip;
        _port = port;
        handlerType = HandlerType.NODE;
        Instances.Add(this);
    }
}
