using Microsoft.AspNetCore.Http;
using System.Text.Json;
using System.Text.Json.Nodes;
using Master.Sessions;
using SessionsSvc = Master.Sessions.Sessions;

namespace Master.Routes
{
    public static partial class Routes
    {
        public static async Task AcceptNode(HttpContext context)
        {
            using var reader = new StreamReader(context.Request.Body);
            string requestBody = await reader.ReadToEndAsync();
            var json = JsonNode.Parse(requestBody);

            try
            {
                Nodes.AcceptNodeReqBody reqBody = JsonSerializer.Deserialize<Nodes.AcceptNodeReqBody>(requestBody, new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                });
                // Validate SessionId
                if (string.IsNullOrWhiteSpace(reqBody.SessionId))
                {
                    context.Response.StatusCode = StatusCodes.Status400BadRequest;
                    context.Response.ContentType = "application/json";
                    await context.Response.WriteAsync(new JsonObject
                    {
                        ["error"] = "SessionId is required"
                    }.ToJsonString());
                    return;
                }
                var (status, body) = Utils.Retry(() => Nodes.AcceptNode(reqBody), 10, 100);
                context.Response.StatusCode = status;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(body.ToJsonString());
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex);
                context.Response.StatusCode = StatusCodes.Status500InternalServerError;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = ex.Message
                }.ToJsonString());
            }
        }

        public static async Task CreateNode(HttpContext context)
        {
            using var reader = new StreamReader(context.Request.Body);
            string requestBody = await reader.ReadToEndAsync();

            try
            {
                var json = JsonNode.Parse(requestBody);

                Nodes.CreateNodeReqBody reqBody = JsonSerializer.Deserialize<Nodes.CreateNodeReqBody>(requestBody, new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                });
                // Validate SessionId
                if (string.IsNullOrWhiteSpace(reqBody.SessionId))
                {
                    context.Response.StatusCode = StatusCodes.Status400BadRequest;
                    context.Response.ContentType = "application/json";
                    await context.Response.WriteAsync(new JsonObject
                    {
                        ["error"] = "SessionId is required"
                    }.ToJsonString());
                    return;
                }

                var (status, body) = Nodes.CreateNode(reqBody);
                context.Response.StatusCode = status;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(body.ToJsonString());
            }
            catch (JsonException ex)
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                context.Response.ContentType = "application/json";

                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = ex.Message
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
            Console.WriteLine("debug 4");
        }


        public static async Task CreateSession(HttpContext context)
        {
            using var reader = new StreamReader(context.Request.Body);
            string requestBody = await reader.ReadToEndAsync();

            var json = JsonNode.Parse(requestBody) ?? new JsonObject();
            var sessionId = json["SessionId"]?.GetValue<string>();

            Console.WriteLine("Creating session with id " + sessionId);

            if (string.IsNullOrWhiteSpace(sessionId))
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = "SessionId is required"
                }.ToJsonString());
                return;
            }

            var (status, body) = await SessionsSvc.CreateSession(sessionId);
            context.Response.StatusCode = status;
            context.Response.ContentType = "application/json";
            await context.Response.WriteAsync(body.ToJsonString());
        }

        public static async Task CleanupSession(HttpContext context)
        {
            using var reader = new StreamReader(context.Request.Body);
            string requestBody = await reader.ReadToEndAsync();

            var json = JsonNode.Parse(requestBody) ?? new JsonObject();
            var sessionId = json["SessionId"]?.GetValue<string>();

            Console.WriteLine("Cleaning session with id " + sessionId);

            if (string.IsNullOrWhiteSpace(sessionId))
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                context.Response.ContentType = "application/json";
                await context.Response.WriteAsync(new JsonObject
                {
                    ["error"] = "SessionId is required"
                }.ToJsonString());
                return;
            }

            var (status, body) = await SessionsSvc.CleanupSession(sessionId);
            context.Response.StatusCode = status;
            context.Response.ContentType = "application/json";
            await context.Response.WriteAsync(body.ToJsonString());
        }

    }
}