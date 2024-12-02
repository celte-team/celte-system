using System;
using System.Threading;

class Master
{
    public SetupConfig? _setupConfig;
    public KafkaManager? kafkaManager;
    private static Master? _master;
    public KFKProducer kFKProducer;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
    public KfkConsumerListener kfkConsumerListener;

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
            _setupConfig = new SetupConfig(Environment.GetCommandLineArgs());
            _setupConfig.SettingUpMaster();
            // kfkConsumerListener = new KfkConsumerListener(_setupConfig.GetYamlObjectConfig()["kafka_brokers"].ToString()
            // , "kafka-dotnet");

            // StartKafkaSystem();
            StartPulsarSystem();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Master: {e.Message}");

        }
    }

    /// <summary>
    /// Start the Kafka system
    /// </summary>
    // public void StartKafkaSystem()
    // {
    //     var consumerThread = new Thread(() => kfkConsumerListener.StartConsuming(cancellationTokenSource.Token));
    //     consumerThread.Start();
    //     var StartExecuteBufferThread = new Thread(() => kfkConsumerListener.StartExecuteBuffer(cancellationTokenSource.Token));

    //     StartExecuteBufferThread.Start();

    //     ConnectNode connectNode = new ConnectNode();
    //     ConnectClient connectClient = new ConnectClient();

    //     kfkConsumerListener.AddTopic(M.Global.MasterHelloSn, connectNode.connectNewNode);
    //     kfkConsumerListener.AddTopic(M.Global.MasterHelloClient, connectClient.connectNewClient);
    //     kfkConsumerListener.AddTopic(M.Global.MasterRPC, null);


    //     kFKProducer = new KFKProducer();
    // }
    public void StartPulsarSystem()
    {
        PulsarProducer pulsarProducer = new PulsarProducer();
        Console.WriteLine("Pulsar system started 🥳");
        pulsarProducer.ProduceMessageAsync("persistent://public/default/mytopic", "Hello World");
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