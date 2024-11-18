using System;
using System.Threading;

class Master
{
    public SetupConfig? _setupConfig;
    private static Master? _master;
    public KFKProducer kFKProducer = null;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
    public KfkConsumerListener kfkConsumerListener;

    public Dictionary<string, Action<byte[]>> topicAction = new Dictionary<string, Action<byte[]>>();

    private Master()
    {
        if (_master != null)
        {
            throw new Exception("Cannot create another instance of Master");
        }
        try
        {
            if (_master == null)
            {
                _master = this;
            }
            // setup the configuration
            _setupConfig = new SetupConfig(Environment.GetCommandLineArgs());
            _setupConfig.SettingUpMaster();

            kfkConsumerListener = new KfkConsumerListener(_setupConfig.GetYamlObjectConfig()["kafka_brokers"].ToString()
            , "kafka-dotnet");
            StartKafkaSystem();
            Console.WriteLine("Master initialized");
        }
        catch (Exception e)
        {
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
        ConnectNode connectNode = new ConnectNode();
        ConnectClient connectClient = new ConnectClient();

        int NumberOfTopics = 3;

        Console.WriteLine("Kafka system started");
        topicAction = new Dictionary<string, Action<byte[]>>
            {
                { M.Global.MasterHelloSn, connectNode.connectNewNode },
                { M.Global.MasterHelloClient, connectClient.connectNewClient },
            };

        foreach (var topic in topicAction)
        {
            kfkConsumerListener.AddTopic(topic.Key, topic.Value, NumberOfTopics);
        }

        // kfkConsumerListener.AddTopic(M.Global.MasterHelloSn, connectNode.connectNewNode, NumberOfTopics);
        // kfkConsumerListener.AddTopic(M.Global.MasterHelloClient, connectClient.connectNewClient, NumberOfTopics);
        // kfkConsumerListener.AddTopic(M.Global.MasterRPC, null, NumberOfTopics);

        kFKProducer = new KFKProducer();
    }

    private void __handleRPC(string message)
    {
        Console.WriteLine($"Received RPC message: {message}");
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