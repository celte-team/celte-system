using System;

namespace M
{
    public static class Global
    {
        private static string masterUUID = "master";
        private static string masterHelloSn = "master.hello.sn";
        private static string masterHelloClient = "master.hello.client";
        private static string masterRPC = "master.rpc";


        private static string pdefault = "persistent://public/default/";


        public static string Pdefault { get { return pdefault; } }
        public static string MasterUUID { get { return masterUUID; } }
        public static string MasterHelloSn { get { return masterHelloSn; } }
        public static string MasterHelloClient { get { return masterHelloClient; } }
        public static string MasterRPC { get { return masterRPC; } }

    }
}