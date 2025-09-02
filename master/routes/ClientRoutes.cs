using Microsoft.AspNetCore.Http;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Master.Routes
{
    public static partial class Routes
    {
        public static async Task AcceptClient(HttpContext context)
        {
            using var reader = new StreamReader(context.Request.Body);
            string requestBody = await reader.ReadToEndAsync();
            var json = JsonNode.Parse(requestBody);

            try
            {
                Clients.ClientConnectToClusterReqBody reqBody = JsonSerializer.Deserialize<Clients.ClientConnectToClusterReqBody>(requestBody, new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                });
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
                var (status, body) = Clients.ConnectClientToCluster(reqBody);
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
        }
    }
}