class KafkaManager
{
    private KFKProducer _producer;
    public KafkaManager()
    {
        Master master = Master.GetInstance();
        _producer = new KFKProducer();
    }
}