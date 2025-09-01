import os
import time
import json
import yaml
from pulsar import Client
import sys

def load_config():
    yaml_path = os.getenv("CELTE_CONFIG", "/root/.celte.yaml")
    if not os.path.exists(yaml_path):
        return {}

    try:
        with open(yaml_path, 'r') as f:
            config = yaml.safe_load(f) or {}
    except Exception:
        return {}

    section = None
    if isinstance(config, dict):
        section = config.get("celte") or config.get("clock") or config.get("master")

    if isinstance(section, list):
        flat_config = {}
        for item in section:
            if isinstance(item, dict):
                flat_config.update(item)
        return flat_config

    if isinstance(section, dict):
        return section

    return config if isinstance(config, dict) else {}


def resolve_pulsar_broker_url(config: dict) -> str:
    """Resolve Pulsar broker URL from env or config.

    Priority:
    1) PULSAR_BROKERS env (full URL or host:port)
    2) CELTE_PULSAR_HOST and CELTE_PULSAR_PORT (env overrides config)
    """
    env_brokers = os.getenv("PULSAR_BROKERS")
    if env_brokers:
        url = env_brokers.strip()
        if not url.startswith("pulsar://"):
            url = f"pulsar://{url}"
        return url

    host = os.getenv("CELTE_PULSAR_HOST") or config.get("CELTE_PULSAR_HOST")
    port = os.getenv("CELTE_PULSAR_PORT") or config.get("CELTE_PULSAR_PORT")
    if host and port:
        return f"pulsar://{host}:{port}"

    raise SystemExit(
        "Pulsar broker not configured. Set PULSAR_BROKERS or CELTE_PULSAR_HOST/CELTE_PULSAR_PORT"
    )

def clock_thread():
    config = load_config()
    pulsar_broker_url = resolve_pulsar_broker_url(config)

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
