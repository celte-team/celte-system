
using DotPulsar;
// using DotPulsar.Abstractions;

class Master
{
    public SetupConfig? _setupConfig;
    private static Master? _master;
    public PulsarConsumer pulsarConsumer;
    public PulsarProducer pulsarProducer;
    public CancellationTokenSource cancellationTokenSource = new CancellationTokenSource();
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
            // DotEnv.Load("../.env");
            _setupConfig = new SetupConfig(Environment.GetCommandLineArgs());
            _setupConfig.SettingUpMaster();
            Redis.RedisClient redisClient = Redis.RedisClient.GetInstance();
            string pulsarBrokers = Environment.GetEnvironmentVariable("PULSAR_BROKERS") ?? string.Empty;
            if (string.IsNullOrEmpty(pulsarBrokers))
            {
                throw new ArgumentException("Pulsar brokers are not set.");
            }
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