#!/usr/bin/env python3

import pulsar
import json
import time

def main():
    client = pulsar.Client('pulsar://3.21.114.171:6650')
    # client = pulsar.Client('pulsar://localhost:6650')
    topic = "portal"

    while True:
        try:
            producer = client.create_producer(topic)
            break
        except pulsar._pulsar.Timeout:
            print("Timeout while creating producer. Retrying in 1 second.")

    try:
        message = f"{{\"action\": \"create\"}}"
        producer.send(message.encode('utf-8'))
        print("message sent!")
        time.sleep(30)
        message = f"{{\"action\": \"delete\"}}"
        producer.send(message.encode('utf-8'))

    except KeyboardInterrupt:
        print("Production interrupted.")

    finally:
        producer.close()
        client.close()
        print("Production finished.")

if __name__ == "__main__":
    main()