public class ReplicationLayerHandler : AHandler
{
    public ReplicationLayerHandler(string ip, int port) : base(ip, port)
    {
        _ip = ip;
        _port = port;
        handlerType = HandlerType.RL;
    }
}
