using Microsoft.AspNetCore.Http;
using System.Text.Json.Nodes;

namespace Master.Routes
{
    public static partial class Routes
    {
        public static async Task ClearRedis(HttpContext context)
        {
            Console.WriteLine("Clearing Redis database...");
            await context.Response.WriteAsync("Clearing Redis database...");
            try
            {
                RedisDb.Database.Execute("FLUSHDB");
                context.Response.StatusCode = StatusCodes.Status200OK;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["message"] = "Redis database cleared successfully."
                }.ToJsonString());
            }
            catch (Exception ex)
            {
                context.Response.StatusCode = StatusCodes.Status500InternalServerError;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = ex.Message
                }.ToJsonString());
            }
        }
    }
}