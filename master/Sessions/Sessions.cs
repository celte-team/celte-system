using Microsoft.AspNetCore.Http;
using System.Text.Json;
using System.Text.Json.Nodes;


namespace Master.Sessions
{
    public static class Sessions
    {
        public static async Task<(int status, JsonNode body)> CreateSession(string sessionId)
        {
            try
            {
                await PulsarSingleton.CreateNamespaceAsync("public", sessionId);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error creating session: {ex.Message}");
                var errorNode = JsonNode.Parse($"{{\"error\": \"{ex.Message}\"}}") ?? new JsonObject { ["error"] = ex.Message };
                return (500, errorNode);
            }

            var sessionNode = JsonNode.Parse($"{{\"sessionId\": \"{sessionId}\"}}") ?? new JsonObject { ["sessionId"] = sessionId };
            return (200, sessionNode);
        }

        public static async Task<(int status, JsonNode body)> CleanupSession(string sessionId)
        {
            try
            {
                // We cleanup any process related to this session
                UpAndDown.CleanupSessionProcesses(sessionId);

                // Remove the namespace
                await PulsarSingleton.DeleteNamespaceAsync("public", sessionId);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error cleaning up session: {ex.Message}");
                var errorNode = JsonNode.Parse($"{{\"error\": \"{ex.Message}\"}}") ?? new JsonObject { ["error"] = ex.Message };
                return (500, errorNode);
            }

            return (200, JsonNode.Parse($"{{\"sessionId\": \"{sessionId}\"}}") ?? new JsonObject { ["sessionId"] = sessionId });
        }
    }
}