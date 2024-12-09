class Master
{
    public SetupConfig? _setupConfig;
    private static Master? _master;
    public PulsarConsumer pulsarConsumer;
    public PulsarProducer pulsarProducer;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
    public RPC rpc = new RPC();
    public string redisIp;

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
            redisIp = _setupConfig.GetYamlObjectConfig()?["redis_host"]?.ToString() ?? "localhost:6379";
            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance(redisIp);
            pulsarConsumer = new PulsarConsumer();
            pulsarProducer = new PulsarProducer();
            StartPulsarConsumer();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Master: {e.Message}");

        }
    }

    /// <summary>
    /// Start the Pulsar consumer required for the Master.
    /// </summary>
    public void StartPulsarConsumer()
    {
        ConnectNode connectNode = new ConnectNode();
        ConnectClient connectClient = new ConnectClient();
        pulsarConsumer.CreateConsumer(new SubscribeOptions
        {
            Topics = "persistent://public/default/" + M.Global.MasterHelloSn,
            SubscriptionName = M.Global.MasterHelloSn,
            Handler = (consumer, message) => connectNode.connectNewNode(message)
        });
        pulsarConsumer.CreateConsumer(new SubscribeOptions
        {
            Topics = "persistent://public/default/" + M.Global.MasterHelloClient,
            SubscriptionName = M.Global.MasterHelloClient,
            Handler = (consumer, message) => connectClient.ConnectNewClient(message)
        });
        rpc.InitConsumer();
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