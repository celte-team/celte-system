public enum HandlerType
{
    CLIENT,
    NODE,
    RL
}

public static class HandlerFactory
{
    public static IHandler CreateHandler(string ip, int port, HandlerType handlerType)
    {
        return handlerType switch
        {
            HandlerType.CLIENT => new ClientHandler(ip, port),
            HandlerType.NODE => new NodeHandler(ip, port),
            HandlerType.RL => new ReplicationLayerHandler(ip, port),
            _ => throw new ArgumentException("Invalid handler type")
        };
    }
}
