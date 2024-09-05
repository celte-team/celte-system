// create a singleton class

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

class Master
{
    public SetupConfig? _setupConfig;
    public KafkaManager? kafkaManager;
    private static Master? _master;
    public KFKProducer KFKProducer;
    public KFKConsumer KFKConsumer;

    /// <summary>
    /// This is the constructor of the Master class
    /// where all begin...
    /// The master is a singleton class, it will:
    ///  - get the config file
    ///  - start the KFKConsumer
    ///  - start the KFKProducer
    /// </summary>
    /// <exception cref="Exception"></exception>
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
            Console.WriteLine("Master initialized...");
            KFKProducer = new KFKProducer();
            KFKConsumer = new KFKConsumer();
        } catch (Exception e) {
            Console.WriteLine($"Error initializing Kafka producer: {e.Message}");
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

    /// <summary>
    /// Start the Master System,
    /// Generate 100 UUID and send them to the Kafka topic,
    /// </summary>
    public void StartMasterSystem()
    {
        // when the Master start, he will produce 100 UUID
        KFKProducer._uuidProducerService.ProduceUUID(1);
    }
}