using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using StackExchange.Redis;
using Master.Routes;
using Microsoft.Extensions.Configuration;

class Program
{
    static async Task Main(string[] args)
    {
        string configPath;
        string? envConfigPath = Environment.GetEnvironmentVariable("CELTE_CONFIG");

        if (!string.IsNullOrWhiteSpace(envConfigPath))
        {
            configPath = envConfigPath;
            Console.WriteLine($"Using config path from CELTE_CONFIG env var: {configPath}");
        }
        else if (args.Length >= 1 && !string.IsNullOrWhiteSpace(args[0]))
        {
            configPath = args[0];
            Console.WriteLine($"Using config path from argument: {configPath}");
        }
        else
        {
            var homeDirectory = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            configPath = Path.Combine(homeDirectory, ".celte.yaml");
            Console.WriteLine($"Using default config path: {configPath}");
        }

        try
        {
            Utils.LoadYamlConfig(configPath);
            PulsarSingleton.InitializeClient();
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Error loading config: {ex.Message}");
            return;
        }

        var host = Host.CreateDefaultBuilder(args)
            .ConfigureWebHostDefaults(webBuilder =>
            {
                webBuilder.ConfigureKestrel(serverOptions =>
                {
                    serverOptions.ListenAnyIP(1908, listenOptions =>
                    {
                        listenOptions.Protocols = Microsoft.AspNetCore.Server.Kestrel.Core.HttpProtocols.Http1;
                    });
                });
                webBuilder.UseStartup<HttpServer.Startup>();
            })
            .Build();

        var lifetime = host.Services.GetRequiredService<IHostApplicationLifetime>();
        lifetime.ApplicationStopped.Register(async () =>
        {
            Console.WriteLine("Application stopped. Cleaning up Redis database...");
            await RedisDb.Database.ExecuteAsync("FLUSHDB");
        });

        // Add cleanup on application shutdown
        lifetime.ApplicationStopping.Register(() =>
        {
            Console.WriteLine("Application is shutting down...");
            UpAndDown.CleanupAllProcesses();
            Console.WriteLine("All processes have been cleaned up.");
        });

        await host.RunAsync();
        await PulsarSingleton.ShutdownAsync();
    }

}

namespace HttpServer
{
    public class Startup
    {
        public static void ConfigureServices(IServiceCollection services)
        {
            services.AddSingleton<IConnectionMultiplexer>(sp => RedisDb.Connection);
            services.AddRouting();
        }

        public void Configure(IApplicationBuilder app, IWebHostEnvironment env)
        {
            if (env.IsDevelopment())
            {
                app.UseDeveloperExceptionPage();
            }

            app.UseRouting();

            app.UseEndpoints(endpoints =>
            {
                endpoints.MapPost("/server/connect", Routes.AcceptNode);
                endpoints.MapPost("/client/link", Routes.AcceptClient);
                endpoints.MapPost("/server/create", Routes.CreateNode);
                endpoints.MapPost("/redis/clear", Routes.ClearRedis);
                endpoints.MapPost("/server/cleanup_session", Routes.CleanupSession);
                // endpoints.MapPost("/master/create", Routes.CreateMaster);
            });
        }

        private static async Task LogToRedis(HttpContext context, string message)
        {
            var redis = context.RequestServices.GetRequiredService<IConnectionMultiplexer>();
            var db = redis.GetDatabase();
            await db.StringAppendAsync("master_logs", $"{DateTime.UtcNow}: {message}\n");
        }
    }
}
