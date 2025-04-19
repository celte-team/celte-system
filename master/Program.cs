using System.Runtime.InteropServices;
using DotNetEnv;
using DotPulsar;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using StackExchange.Redis;

class Program
{
    static async Task Main(string[] args)
    {
        Env.Load();

        PulsarSingleton.InitializeClient();

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