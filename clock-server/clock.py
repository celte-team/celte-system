import os
import time
import json
from pulsar import Client

def clock_thread():
    global client
    pulsar_broker_url = os.getenv('PULSAR_BROKERS', 'pulsar://localhost:6650')
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
