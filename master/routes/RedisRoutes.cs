using Microsoft.AspNetCore.Http;
using System.Text.Json.Nodes;

namespace Master.Routes
{
    public static partial class Routes
    {
        public static async Task ClearRedis(HttpContext context)
        {
            var sessionId = context.Request.Query["SessionId"].FirstOrDefault();
            if (string.IsNullOrWhiteSpace(sessionId))
            {
                try
                {
                    context.Request.EnableBuffering();
                    using var reader = new StreamReader(context.Request.Body, leaveOpen: true);
                    var bodyText = await reader.ReadToEndAsync();
                    context.Request.Body.Position = 0;
                    if (!string.IsNullOrWhiteSpace(bodyText))
                    {
                        var node = JsonNode.Parse(bodyText) as JsonObject;
                        if (node != null)
                        {
                            if (node.TryGetPropertyValue("SessionId", out var s) && s != null)
                                sessionId = s.ToString();
                            else if (node.TryGetPropertyValue("sessionId", out var s2) && s2 != null)
                                sessionId = s2.ToString();
                        }
                    }
                }
                catch { /* ignore body parse errors and fall through to missing param handling */ }
            }
            if (string.IsNullOrWhiteSpace(sessionId))
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = "Missing required query parameter 'sessionId'"
                }.ToJsonString());
                return;
            }

            Console.WriteLine($"Clearing Redis keys for session: {sessionId}");
            try
            {
                RedisDb.ClearSession(sessionId);
            }
            catch (Exception ex)
            {
                context.Response.StatusCode = StatusCodes.Status500InternalServerError;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = ex.Message
                }.ToJsonString());
                return;
            }

            context.Response.StatusCode = StatusCodes.Status200OK;
            context.Response.ContentType = "application/json";
            await context.Response.WriteAsync(new JsonObject
            {
                ["message"] = $"Redis keys for session '{sessionId}' cleared successfully."
            }.ToJsonString());
        }
    }
}