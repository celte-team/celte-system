namespace Redis {
    partial class RedisClient
    {
        // subscriber to a channel
        public void Subscribe(string channel, Action<string, string> handler)
        {
            var subscriber = _connection.GetSubscriber();
            subscriber.Subscribe(channel, (channel, message) => handler(channel, message));
        }

        public void Publish(string channel, string message)
        {
            var publisher = _connection.GetSubscriber();
            publisher.Publish(channel, message);
        }

        public void Unsubscribe(string channel)
        {
            var subscriber = _connection.GetSubscriber();
            subscriber.Unsubscribe(channel);
        }
    }
}