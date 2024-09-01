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
    public static Master GetInstance()
    {
        if (_master == null)
        {
            _master = new Master();
        }

        return _master;
    }

    public void setupKafkaManager()
    {
        // the creation of the KafkaManager is delayed until the config is load (GetYamlObjectConfig())
        kafkaManager = new KafkaManager();
    }
}