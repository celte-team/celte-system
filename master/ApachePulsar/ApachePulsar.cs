using System.Text;
using System.Linq;
using DotPulsar;
using DotPulsar.Extensions;
using DotPulsar.Abstractions;
using System.Buffers;
using System.Collections.Concurrent;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using System.Net;

public class PulsarSingleton
{
    private static DotPulsar.PulsarClient? _client;
    private static readonly Dictionary<string, IProducer<ReadOnlySequence<byte>>> _producers = new();
    private static readonly Dictionary<string, IConsumer<ReadOnlySequence<byte>>> _consumers = new();

    // thread-safe set of namespaces (key -> unused byte)
    private static readonly ConcurrentDictionary<string, byte> _namespaces = new();

    public static void InitializeClient()
    {
        string pulsarBrokers = Utils.GetConfigOption("CELTE_PULSAR_HOST", string.Empty);
        string pulsarPort = Utils.GetConfigOption("CELTE_PULSAR_PORT", "6650");
        pulsarBrokers = "pulsar://" + pulsarBrokers + ":" + pulsarPort;
        if (string.IsNullOrEmpty(pulsarBrokers))
        {
            throw new ArgumentException("Pulsar brokers are not set.");
        }

        _client = (DotPulsar.PulsarClient)DotPulsar.PulsarClient.Builder()
                .ServiceUrl(new Uri(pulsarBrokers))
                .Build();
    }

    public static IProducer<ReadOnlySequence<byte>> GetProducer(string topic)
    {
        if (_client == null)
        {
            throw new InvalidOperationException("Pulsar client is not initialized.");
        }

        if (!_producers.ContainsKey(topic))
        {
            var producer = _client.NewProducer()
                .Topic(topic)
                .Create();
            Console.WriteLine($"Producer created for topic: {topic}");
            _producers[topic] = producer;
        }

        return _producers[topic];
    }

    public static IConsumer<ReadOnlySequence<byte>> GetConsumer(string topic, string subscriptionName, SubscriptionType subscriptionType = SubscriptionType.Shared)
    {
        if (_client == null)
        {
            throw new InvalidOperationException("Pulsar client is not initialized.");
        }

        var key = $"{topic}:{subscriptionName}";
        if (!_consumers.ContainsKey(key))
        {
            var consumer = _client.NewConsumer()
                .Topic(topic)
                .SubscriptionName(subscriptionName)
                .SubscriptionType(subscriptionType)
                .Create();
            _consumers[key] = consumer;
        }

        return _consumers[key];
    }

    public static async Task ProduceMessageAsync(string topic, string message)
    {
        var producer = GetProducer(topic);
        var messageBytes = Encoding.UTF8.GetBytes(message);
        var readOnlySequence = new ReadOnlySequence<byte>(messageBytes);
        await producer.Send(readOnlySequence);
    }

    public static async Task ConsumeMessagesAsync(string topic, string subscriptionName, Action<string> messageHandler, CancellationToken cancellationToken)
    {
        var consumer = GetConsumer(topic, subscriptionName);

        await foreach (var message in consumer.Messages(cancellationToken))
        {
            var data = message.Data.ToArray();
            var messageString = Encoding.UTF8.GetString(data);
            messageHandler(messageString);
            await consumer.Acknowledge(message, cancellationToken);
        }
    }

    public static async Task ShutdownAsync()
    {
        if (_client != null)
        {
            await _client.DisposeAsync();
            _client = null;
        }

        _producers.Clear();
        _consumers.Clear();
    }

    // --- Pulsar Admin helpers ------------------------------------------------
    // These helpers call the Pulsar Admin REST API (default port 30080) to create
    // namespaces. Configure the admin base URL with the CELTE_PULSAR_ADMIN_URL
    // config option (e.g. http://pulsar-broker:30080). If Pulsar is secured with
    // token auth for the admin API, set CELTE_PULSAR_ADMIN_TOKEN to a valid token.

    public static async Task CreateNamespaceAsync(string tenant, string ns)
    {
        string pulsarBrokers = Utils.GetConfigOption("CELTE_PULSAR_HOST", string.Empty);
        int pulsarAdminPort = int.Parse(Utils.GetConfigOption("CELTE_PULSAR_ADMIN_PORT", "30080"));
        var adminUrl = Utils.GetConfigOption("CELTE_PULSAR_ADMIN_URL", $"http://{pulsarBrokers}:{pulsarAdminPort}").TrimEnd('/');
        var uri = new Uri($"{adminUrl}/admin/v2/namespaces/{tenant}/{ns}");

        using var http = new HttpClient();
        var token = Utils.GetConfigOption("CELTE_PULSAR_ADMIN_TOKEN", string.Empty);
        if (!string.IsNullOrWhiteSpace(token))
            http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);

        // The admin API uses PUT to create a namespace. Some Pulsar versions expect
        // an application/json content-type. Send a minimal JSON object to be safe.
        var response = await http.PutAsync(uri, new StringContent("{}", Encoding.UTF8, "application/json"));
        if (!response.IsSuccessStatusCode)
        {
            var body = await response.Content.ReadAsStringAsync();
            throw new InvalidOperationException($"Failed to create Pulsar namespace {tenant}/{ns}: {response.StatusCode} {body}");
        }
        var fullNs = $"{tenant}/{ns}";
        _namespaces.TryAdd(fullNs, 0);
    }

    public static Task DeleteNamespaceAsync(string tenant, string ns)
    {
        // Implemented as async method to perform admin REST calls and local cleanup.
        return DeleteNamespaceImplAsync(tenant, ns);

        async Task DeleteNamespaceImplAsync(string t, string n)
        {
            string pulsarBrokers = Utils.GetConfigOption("CELTE_PULSAR_HOST", string.Empty);
            int pulsarAdminPort = int.Parse(Utils.GetConfigOption("CELTE_PULSAR_ADMIN_PORT", "30080"));
            var adminUrl = Utils.GetConfigOption("CELTE_PULSAR_ADMIN_URL", $"http://{pulsarBrokers}:{pulsarAdminPort}").TrimEnd('/');

            using var http = new HttpClient();
            var token = Utils.GetConfigOption("CELTE_PULSAR_ADMIN_TOKEN", string.Empty);
            if (!string.IsNullOrWhiteSpace(token))
                http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);

            // 1) List topics in the namespace
            var listUri = new Uri($"{adminUrl}/admin/v2/namespaces/{t}/{n}/topics");
            var listResp = await http.GetAsync(listUri);
            if (!listResp.IsSuccessStatusCode)
            {
                var body = await listResp.Content.ReadAsStringAsync();
                throw new InvalidOperationException($"Failed to list Pulsar topics for namespace {t}/{n}: {listResp.StatusCode} {body}");
            }

            var listJson = await listResp.Content.ReadAsStringAsync();
            string[] topics;
            try
            {
                topics = JsonSerializer.Deserialize<string[]>(listJson) ?? Array.Empty<string>();
            }
            catch
            {
                topics = Array.Empty<string>();
            }

            // Helper to strip scheme like "persistent://" or "non-persistent://"
            static string StripScheme(string full)
            {
                if (full.StartsWith("non-persistent://")) return full.Substring("non-persistent://".Length);
                if (full.StartsWith("non-persistent://")) return full.Substring("non-persistent://".Length);
                return full;
            }

            // Helper to detect if a given topic key (from local dictionaries) belongs to the tenant/ns
            static bool TopicMatchesNamespace(string topicKey, string tenant, string ns, IEnumerable<string> knownTopics)
            {
                if (string.IsNullOrEmpty(topicKey)) return false;
                // Direct matches
                if (topicKey.Equals($"{tenant}/{ns}", StringComparison.OrdinalIgnoreCase)) return true;
                // If topicKey contains scheme, strip it
                var strippedKey = topicKey.StartsWith("non-persistent://") || topicKey.StartsWith("non-persistent://") ? topicKey : topicKey;
                // If local key contains tenant/ns segment
                if (topicKey.Contains($"{tenant}/{ns}/")) return true;
                // If any known admin topic string matches (full or stripped)
                foreach (var kt in knownTopics)
                {
                    if (string.IsNullOrEmpty(kt)) continue;
                    var s = StripScheme(kt);
                    if (s.Equals(topicKey, StringComparison.OrdinalIgnoreCase)) return true;
                    if (s.EndsWith(topicKey, StringComparison.OrdinalIgnoreCase)) return true;
                    if (topicKey.EndsWith(s, StringComparison.OrdinalIgnoreCase)) return true;
                }
                return false;
            }

            // 2) Disconnect matching local producers
            var producerKeys = _producers.Keys.ToList();
            foreach (var pkey in producerKeys)
            {
                try
                {
                    if (TopicMatchesNamespace(pkey, t, n, topics))
                    {
                        var p = _producers[pkey];
                        try { await p.DisposeAsync(); } catch { }
                        _producers.Remove(pkey);
                    }
                }
                catch { }
            }

            // 3) Disconnect matching local consumers
            var consumerKeys = _consumers.Keys.ToList();
            foreach (var ckey in consumerKeys)
            {
                try
                {
                    var topicPart = ckey;
                    var idx = ckey.IndexOf(':');
                    if (idx >= 0) topicPart = ckey.Substring(0, idx);
                    if (TopicMatchesNamespace(topicPart, t, n, topics))
                    {
                        var c = _consumers[ckey];
                        try { await c.DisposeAsync(); } catch { }
                        _consumers.Remove(ckey);
                    }
                }
                catch { }
            }

            // 4) Delete schemas and topics reported by admin API
            foreach (var fullTopic in topics)
            {
                if (string.IsNullOrWhiteSpace(fullTopic)) continue;
                var stripped = StripScheme(fullTopic); // tenant/ns/topic
                var parts = stripped.Split('/');
                if (parts.Length < 3) continue;
                var tenantPart = parts[0];
                var nsPart = parts[1];
                // topic may contain additional slashes; join remaining parts
                var topicPart = string.Join('/', parts.Skip(2));
                var encodedTopic = Uri.EscapeDataString(topicPart);
                var is_persistent = fullTopic.StartsWith("persistent://", StringComparison.OrdinalIgnoreCase);
                var basePath = is_persistent ? "persistent" : "non-persistent";

                // delete schema (ignore 404)
                try
                {
                    var schemaUri = new Uri($"{adminUrl}/admin/v2/schemas/{tenantPart}/{nsPart}/{encodedTopic}/schema");
                    var schemaResp = await http.DeleteAsync(schemaUri);
                    if (!schemaResp.IsSuccessStatusCode && schemaResp.StatusCode != HttpStatusCode.NotFound)
                    {
                        var body = await schemaResp.Content.ReadAsStringAsync();
                        throw new InvalidOperationException($"Failed to delete schema for {fullTopic}: {schemaResp.StatusCode} {body}");
                    }
                }
                catch { /* continue trying to delete topic even if schema delete fails */ }

                // delete topic (force)
                try
                {
                    var deleteTopicUri = new Uri($"{adminUrl}/admin/v2/{basePath}/{tenantPart}/{nsPart}/{encodedTopic}?force=true");
                    var delTopicResp = await http.DeleteAsync(deleteTopicUri);
                    if (!delTopicResp.IsSuccessStatusCode && delTopicResp.StatusCode != HttpStatusCode.NotFound)
                    {
                        var body = await delTopicResp.Content.ReadAsStringAsync();
                        throw new InvalidOperationException($"Failed to delete topic {fullTopic}: {delTopicResp.StatusCode} {body}");
                    }
                }
                catch
                {
                    // rethrow to surface failures
                    throw;
                }
            }

            // 5) Delete the namespace itself. Try once plain, then with force if needed.
            var deleteNsUri = new Uri($"{adminUrl}/admin/v2/namespaces/{t}/{n}");
            var delNsResp = await http.DeleteAsync(deleteNsUri);
            if (!delNsResp.IsSuccessStatusCode)
            {
                var forceUri = new Uri($"{adminUrl}/admin/v2/namespaces/{t}/{n}?force=true");
                var forceResp = await http.DeleteAsync(forceUri);
                if (!forceResp.IsSuccessStatusCode)
                {
                    var body = await forceResp.Content.ReadAsStringAsync();
                    throw new InvalidOperationException($"Failed to delete namespace {t}/{n}: {forceResp.StatusCode} {body}");
                }
            }

            // 6) Remove from tracked namespaces
            var fullNs = $"{t}/{n}";
            _namespaces.TryRemove(fullNs, out _);
        }

    }

    public static async Task DeleteAllNamespacesAsync(string tenant)
    {
        // _namespaces stores entries as "tenant/ns"; delete only those that
        // belong to the requested tenant.
        var keys = _namespaces.Keys.ToList();
        foreach (var fullNs in keys)
        {
            if (!fullNs.StartsWith(tenant + "/"))
                continue;
            var nsPart = fullNs.Substring(tenant.Length + 1);
            await DeleteNamespaceAsync(tenant, nsPart);
        }
    }
}
