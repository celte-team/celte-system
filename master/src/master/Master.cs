using DotPulsar;

class Master
{
    public SetupConfig? _setupConfig;
    private static Master? _master;
    public PulsarConsumer pulsarConsumer;
    public PulsarProducer pulsarProducer;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
    public Celte.Master.K8s.K8s k8s;
    public RPC rpc = new RPC();
    private readonly DotPulsar.PulsarClient _client;

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
            // SetupConfig
            _setupConfig = new SetupConfig(Environment.GetCommandLineArgs());
            _setupConfig.SettingUpMaster();

            // Redis
            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();

            // K8s
            // Check if production mode is enabled
            if (_setupConfig.IsProduction())
            {
                try {
                    k8s = new Celte.Master.K8s.K8s(_setupConfig.GetYamlObjectConfig());
                    k8s.IsDeploymentReady();
                } catch (Exception e) {
                    Console.WriteLine($"Error initializing Kubernetes client: {e.Message}");
                    // Decide if this should be fatal or if the application can continue
                }
            } else {
                Console.WriteLine("Production mode is disabled.");
                // read
                //
            }
            // Pulsar
            string pulsarBrokers = Environment.GetEnvironmentVariable("PULSAR_BROKERS") ?? string.Empty;
            Console.WriteLine($"Pulsar brokers: {pulsarBrokers}");
            if (string.IsNullOrEmpty(pulsarBrokers))
            {
                throw new ArgumentException("Pulsar brokers are not set.");
            }
            // Pulsar Client
            _client = (PulsarClient)PulsarClient.Builder()
                .ServiceUrl(new Uri(pulsarBrokers))
                .Build();
            pulsarConsumer = new PulsarConsumer();
            pulsarProducer = new PulsarProducer();
            StartPulsarConsumer();
        }
        catch (Exception e)
        {
            Console.WriteLine($"Error initializing Master: {e.Message}");
            throw e;
        }
    }

    public PulsarClient GetPulsarClient()
    {
        return _client;
    }

    /// <summary>
    /// Start the Pulsar consumer required for the Master.
    /// </summary>
    public void StartPulsarConsumer()
    {
        try {
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
        } catch (Exception e) {
            Console.WriteLine($"Error starting Pulsar consumer: {e.Message}");
            throw e;
        }
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