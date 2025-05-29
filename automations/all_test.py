import os
import time
import sys
import pulsar
import redis
import http.client
import argparse
import matplotlib.pyplot as plt
import statistics
import requests

# Configuration depuis l'environnement ou fallback
PULSAR_SERVICE_URL = os.getenv('PULSAR_BROKERS', 'pulsar://57.128.60.39:32102')
REDIS_HOST = os.getenv('REDIS_HOST', '57.128.60.39')
MASTER_HOST = os.getenv('CELTE_MASTER_HOST', '57.128.60.39')
MASTER_PORT = int(os.getenv('CELTE_MASTER_PORT', '1908'))
LOBBY_HOST = os.getenv('CELTE_LOBBY_HOST', '57.128.60.39')  # à adapter si lobby sur autre IP
LOBBY_PORT = int(os.getenv('CELTE_LOBBY_PORT', '5002'))     # à adapter si lobby sur autre port
PUSHGATEWAY_HOST = os.getenv('PUSHGATEWAY_HOST', '57.128.60.39')
PUSHGATEWAY_PORT = int(os.getenv('PUSHGATEWAY_PORT', '30091'))

def ms_duration(start, end):
    return int((end - start) * 1000)

# --- PULSAR TEST ---
def test_pulsar():
    print(f"\n[TEST] Connexion à Pulsar: {PULSAR_SERVICE_URL}")
    client = None
    start = time.monotonic()
    try:
        client = pulsar.Client(PULSAR_SERVICE_URL)
        topic = 'persistent://public/default/test-topic'
        producer = client.create_producer(topic)
        producer.send(b"ping-test")
        producer.close()
        end = time.monotonic()
        print(f"[OK] Connexion client Pulsar réussie. Ping: {ms_duration(start, end)} ms")
        return True, ms_duration(start, end)
    except Exception as e:
        end = time.monotonic()
        print(f"[FAIL] Pulsar: {e} (après {ms_duration(start, end)} ms)")
        return False, None
    finally:
        if client:
            client.close()

# --- REDIS TEST ---
def test_redis():
    print(f"\n[TEST] Connexion à Redis: {REDIS_HOST}")
    start = time.monotonic()
    try:
        r = redis.Redis(host=REDIS_HOST, port=6379, socket_connect_timeout=5)
        # info = r.info('replication')
        # print(f"Redis role: {info.get('role')}")
        # if info.get('role') != 'master':
        #     end = time.monotonic()
        #     print(f"[FAIL] Redis: Ce n'est pas le master, impossible d'écrire. (après {ms_duration(start, end)} ms)")
        #     return False, None
        pong_start = time.monotonic()
        pong = r.ping()
        pong_end = time.monotonic()
        if pong:
            r.set('test-key', 'test-value')
            val = r.get('test-key')
            print(f"[OK] Connexion Redis réussie (PING). Ping: {ms_duration(pong_start, pong_end)} ms")
            print(f"[OK] Lecture clé test: {val}")
            return True, ms_duration(pong_start, pong_end)
        else:
            end = time.monotonic()
            print(f"[FAIL] Redis: Pas de réponse au PING. (après {ms_duration(start, end)} ms)")
            return False, None
    except Exception as e:
        end = time.monotonic()
        print(f"[FAIL] Redis: {e} (après {ms_duration(start, end)} ms)")
        return False, None

# --- MASTER TEST ---
def test_master():
    print(f"\n[TEST] Connexion HTTP à Master: {MASTER_HOST}:{MASTER_PORT}")
    start = time.monotonic()
    try:
        conn = http.client.HTTPConnection(MASTER_HOST, MASTER_PORT, timeout=5)
        conn.request("POST", "/server/connect")
        resp = conn.getresponse()
        end = time.monotonic()
        print(f"[OK] Master HTTP status: {resp.status} {resp.reason}. Ping: {ms_duration(start, end)} ms")
        conn.close()
        return True, ms_duration(start, end)
    except Exception as e:
        end = time.monotonic()
        print(f"[FAIL] Master: {e} (après {ms_duration(start, end)} ms)")
        return False, None

# --- LOBBY TEST ---
def test_lobby():
    print(f"\n[TEST] Connexion HTTP à Lobby: {LOBBY_HOST}:{LOBBY_PORT}")
    start = time.monotonic()
    try:
        conn = http.client.HTTPConnection(LOBBY_HOST, LOBBY_PORT, timeout=5)
        conn.request("GET", "/")
        resp = conn.getresponse()
        end = time.monotonic()
        print(f"[OK] Lobby HTTP status: {resp.status} {resp.reason}. Ping: {ms_duration(start, end)} ms")
        conn.close()
        return True, ms_duration(start, end)
    except Exception as e:
        end = time.monotonic()
        print(f"[FAIL] Lobby: {e} (après {ms_duration(start, end)} ms)")
        return False, None

# --- PUSHGATEWAY TEST ---
def test_pushgateway():
    job_name = "test_pushgateway_job"
    instance_name = "test_instance"
    pushgateway_url = f"http://{PUSHGATEWAY_HOST}:{PUSHGATEWAY_PORT}/metrics/job/{job_name}/instance/{instance_name}"
    print(f"\n[TEST] Connexion à Pushgateway: {pushgateway_url}")
    start = time.monotonic()
    try:
        # Simple metric in Prometheus text format
        metric_data = "my_custom_metric 123\n"
        response = requests.post(pushgateway_url, data=metric_data, headers={'Content-Type': 'text/plain'}, timeout=5)
        end = time.monotonic()

        # Pushgateway usually returns 200 or 202 on successful push
        if response.status_code == 200 or response.status_code == 202:
            print(f"[OK] Pushgateway: Métrique poussée avec succès. Status: {response.status_code}. Ping: {ms_duration(start, end)} ms")
            # Optionally, try to delete the metric group to keep Pushgateway clean
            try:
                delete_response = requests.delete(pushgateway_url, timeout=2)
                if delete_response.status_code == 202:
                    print(f"[INFO] Pushgateway: Groupe de métriques supprimé avec succès.")
                else:
                    print(f"[WARN] Pushgateway: Échec de la suppression du groupe de métriques, status: {delete_response.status_code}")
            except Exception as del_e:
                print(f"[WARN] Pushgateway: Erreur lors de la suppression du groupe de métriques: {del_e}")
            return True, ms_duration(start, end)
        else:
            print(f"[FAIL] Pushgateway: Échec de la poussée de la métrique. Status: {response.status_code} {response.reason}. (après {ms_duration(start, end)} ms)")
            return False, None
    except requests.exceptions.RequestException as e:
        end = time.monotonic()
        print(f"[FAIL] Pushgateway: {e} (après {ms_duration(start, end)} ms)")
        return False, None

def benchmark_pulsar(messages=1000, payload_size=100):
    print(f"\n[Benchmark] Stress test Pulsar: {messages} messages, payload {payload_size} bytes")
    client = None
    latencies = []
    try:
        client = pulsar.Client(PULSAR_SERVICE_URL)
        topic = 'persistent://public/default/benchmark-topic'
        producer = client.create_producer(topic)
        payload = b'x' * payload_size
        progress_step = messages // 10 if messages >= 10 else None
        for i in range(messages):
            start = time.monotonic()
            producer.send(payload)
            end = time.monotonic()
            latencies.append((end - start) * 1000)  # ms
            if progress_step and (i+1) % progress_step == 0:
                print(f"  {i+1}/{messages} messages sent...")
        producer.close()
        print("[Benchmark] Finished sending messages.")
        if latencies:
            median_latency = statistics.median(latencies)
            print(f"[Benchmark] Median ping time: {median_latency:.2f} ms")
        return latencies
    except Exception as e:
        print(f"[FAIL] Benchmark Pulsar: {e}")
        return None
    finally:
        if client:
            client.close()

def plot_latencies(latencies, messages, payload, output_file="pulsar_benchmark.png"):
    plt.figure(figsize=(10,5))
    plt.plot(latencies, label="Latency (ms)")
    if latencies:
        median_latency = statistics.median(latencies)
        plt.axhline(median_latency, color='r', linestyle='--', label=f"Median: {median_latency:.2f} ms")
    plt.xlabel("Message #")
    plt.ylabel("Latency (ms)")
    plt.title(f"Pulsar Benchmark - Message Latency\nMessages: {messages}, Payload: {payload} bytes")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(output_file)
    print(f"[Benchmark] Graph saved to {output_file}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--benchmark', action='store_true', help="Run Pulsar stress test and generate graph")
    parser.add_argument('--messages', type=int, default=1000, help="Number of messages for benchmark")
    parser.add_argument('--payload', type=int, default=100, help="Payload size in bytes for benchmark")
    args = parser.parse_args()

    if args.benchmark:
        latencies = benchmark_pulsar(messages=args.messages, payload_size=args.payload)
        if latencies:
            plot_latencies(latencies, args.messages, args.payload)
        else:
            sys.exit(1)
    else:
        results = {
            'pulsar': test_pulsar(),
            'redis': test_redis(),
            'master': test_master(),
            'lobby': test_lobby(),
            'pushgateway': test_pushgateway(),
        }
        print("\nRésumé des tests:")
        for k, (v, ping) in results.items():
            if v:
                print(f"  {k}: OK ({ping} ms)")
            else:
                print(f"  {k}: FAIL")
        if not all(v for v, _ in results.values()):
            sys.exit(1)