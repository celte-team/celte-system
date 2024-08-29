public interface IHandler
{
    void HandleMessage(byte[] message);
    (string?, string?, byte[]?) TryPopMessage();
    void PopAllMessages(Queue<(string?, string?, byte[]?)> queue);
}