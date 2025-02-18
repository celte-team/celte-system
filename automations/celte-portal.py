#!/usr/bin/env python3

import pulsar
import time
import json
import uuid
import os
import subprocess
import docker

client = ""

def create_cluster():
    cluster_uuid = uuid.uuid4()
    print(f"Cluster created with UUID: {cluster_uuid}")
    docker_client = docker.from_env()
    docker_client.containers.run("clmt/celte-master:latest", name=f"celte-master-service-{cluster_uuid}", environment=["DOCKER_HOST_IP=192.168.0.161", "PULSAR_BROKERS=3.21.114.171", "REDIS_HOST=192.168.0.161"], detach=True)

    # create a new pulsar namespace with the cluster_uuid
    global client
    # admin.namespaces().create_namespace('public', f'celte-cluster-{cluster_uuid}')

    # bin/pulsar-admin namespaces create public/celestial-cluster-{cluster_uuid}


def delete_cluster(uuid):
    print(f"Cluster deleted with UUID: {uuid}")
    docker_client = docker.from_env()
    try:
        container = docker_client.containers.get(f"celte-master-service-{uuid}")
        container.stop()
        container.remove()
    except docker.errors.APIError as e:
        print(f"Error deleting container: {e}")

def compute_message(message):
    if message["action"] == "create":
        print("Creating a new cluster...")
        create_cluster()
    elif message["action"] == "delete":
        print("Deleting the cluster...")
        delete_cluster(message["uuid"])  # Pass the uuid to delete
    elif message["action"] == "join":
        print("Joining the cluster... and sending back a UUID")
    else:
        print("Unknown action.")

def main():
    global client
    client = pulsar.Client('pulsar://3.21.114.171:6650')

    # Topic name
    topic = "portal"

    # Creating the consumer
    while True:
        try:
            consumer = client.subscribe(topic, "portal")
            break
        except pulsar._pulsar.Timeout:
            print("Timeout while creating consumer. Retrying in 1 second.")
            time.sleep(1)

    try:
        # Consuming multiple messages
        while True:
            message = consumer.receive()
            print(f"Consumed message: {message.data().decode('utf-8')}")
            consumer.acknowledge(message)
            compute_message(json.loads(message.data().decode('utf-8')))
    except KeyboardInterrupt:
        print("Production interrupted.")
    finally:
        consumer.close()
        client.close()
        print("Production finished.")

if __name__ == "__main__":
    main()
