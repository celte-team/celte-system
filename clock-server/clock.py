import os
import time
import json
import yaml
from pulsar import Client
import sys

def load_config():
    yaml_path = os.getenv("CELTE_CONFIG", "/root/.celte.yaml")
    with open(yaml_path, 'r') as f:
        config = yaml.safe_load(f)
    # Fallback to 'master' section if 'clock' not defined
    raw_config = config.get("celte") or config.get("clock") or config.get("master")

    if isinstance(raw_config, list):
        # Convert list of dicts to a single flat dict
        flat_config = {}
        for item in raw_config:
            flat_config.update(item)
        return flat_config

    return raw_config or {}

def clock_thread():
    global client
    config = load_config()
    pulsar_broker_host = config.get("CELTE_PULSAR_HOST", "localhost")
    pulsar_broker_port = config.get("CELTE_PULSAR_PORT", "6650")
    pulsar_broker_url = f"pulsar://{pulsar_broker_host}:{pulsar_broker_port}"

    print(f"Connecting to Pulsar broker at {pulsar_broker_url}")
    client = Client(pulsar_broker_url)
    producer = client.create_producer('persistent://public/default/global.clock')
    while True:
        time.sleep(1)  # 1 second
        msg = {
            "unified_time_ms": int(time.time() * 1000)
        }
        producer.send(json.dumps(msg).encode('utf-8'))
        producer.flush()

if __name__ == "__main__":
    clock_thread()
