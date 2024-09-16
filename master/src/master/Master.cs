using System;
using System.Threading;

class Master
{
    public SetupConfig? _setupConfig;
    public KafkaManager? kafkaManager;
    private static Master? _master;
    public KFKProducer kFKProducer;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
    // public KfkConsumerListener kfkConsumerListener = new KfkConsumerListener("localhost:80", "kafka-dotnet-getting-started");
    public KfkConsumerListener kfkConsumerListener;

    private Master()
    {
        if (_master != null)
        {
            throw new Exception("Cannot create another instance of Master");
        }
        try {
            if (_master == null)
            {
                _master = this;
            }
            _setupConfig = new SetupConfig(Environment.GetCommandLineArgs());
            _setupConfig.SettingUpMaster();
            kfkConsumerListener = new KfkConsumerListener(_setupConfig.GetYamlObjectConfig()["kafka_brokers"].ToString()
            , "kafka-dotnet-getting-started");

            StartKafkaSystem();
        } catch (Exception e) {
            Console.WriteLine($"Error initializing Master: {e.Message}");
        }
    }

    /// <summary>
    /// Start the Kafka system
    /// </summary>
    public void StartKafkaSystem()
    {
        var consumerThread = new Thread(() => kfkConsumerListener.StartConsuming(cancellationTokenSource.Token));
        consumerThread.Start();
        var StartExecuteBufferThread = new Thread(() => kfkConsumerListener.StartExecuteBuffer(cancellationTokenSource.Token));

        StartExecuteBufferThread.Start();

        //from UUIDConsumer.cs
        UUIDConsumerService uuidConsumerService = new UUIDConsumerService();
        ConnectNode connectNode = new ConnectNode();
        ConnectClient connectClient = new ConnectClient();

        kfkConsumerListener.AddTopic("master.hello.sn", connectNode.connectNewNode);

        kfkConsumerListener.AddTopic("master.hello.client", connectClient.connectNewClient);

        //from UUIDProducer.cs
        kFKProducer = new KFKProducer();
        // produce 100 UUIDs.
        kFKProducer._uuidProducerService.ProduceUUID(10);
    }

    public static Master GetInstance()
    {
        if (_master == null)
        {
            _master = new Master();
        }
        return _master;
    }

    public void Dispose()
    {
        _setupConfig?.Shutdown();
        cancellationTokenSource.Cancel();
    }

    ~Master()
    {
        Dispose();
    }
}
