    public enum OpcodeEnum : int
    {

        // Master: 0 -> 99
        PING = 0,
        NEW_NODE = 1,
        AUTHORITY_TRANSFER = 2,
        WARN_CLIENT_AUTHORITY_TRANSFER = 3,
        ASSIGN_CLIENT_TO_NODE = 5,


        //Replication Layer: 100 -> 199
        GAME_STATE = 101,


        //Node: 200 -> 299
        HELLO_SN = 200,


        //Client: 300 -> 399
        HELLO_CLIENT = 300,
    }
