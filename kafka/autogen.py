#!/usr/bin/env python

import sys
import os
import subprocess

def generate_ssl_certificates(cert_dir, domain):
    if os.path.exists(cert_dir):
        subprocess.run(["rm", "-rf", cert_dir], check=True)
    if not os.path.exists(cert_dir):
        os.makedirs(cert_dir)
    if not os.path.exists(os.path.join(cert_dir, "haproxy")):
        os.makedirs(os.path.join(cert_dir, "haproxy"))
    if not os.path.exists(os.path.join(cert_dir, "kafka")):
        os.makedirs(os.path.join(cert_dir, "kafka"))

    key_file = os.path.join(cert_dir, f"{domain}.key")
    csr_file = os.path.join(cert_dir, f"{domain}.csr")
    crt_file = os.path.join(cert_dir, f"{domain}.crt")
    combined_file = os.path.join(cert_dir, "haproxy", f"{domain}.pem")

    # Generate private key
    subprocess.run([
        "openssl", "genpkey", "-algorithm", "RSA", "-out", key_file, "-pkeyopt", "rsa_keygen_bits:2048"
    ], check=True)

    # Generate CSR
    subprocess.run([
        "openssl", "req", "-new", "-key", key_file, "-out", csr_file,
        "-subj", f"/CN={domain}"
    ], check=True)

    # Generate self-signed certificate
    subprocess.run([
        "openssl", "x509", "-req", "-days", "365", "-in", csr_file, "-signkey", key_file, "-out", crt_file
    ], check=True)

    # Combine the certificate and key into a single file
    with open(combined_file, 'w') as f:
        with open(crt_file, 'r') as crt:
            f.write(crt.read())
        with open(key_file, 'r') as key:
            f.write(key.read())

    print(f"SSL certificates generated successfully in {cert_dir}")

    # Generate truststore and keystore
    truststore_file = os.path.join(cert_dir, "kafka", "kafka.truststore.jks")
    keystore_file = os.path.join(cert_dir, "kafka", "kafka.keystore.jks")

    subprocess.run([
        "keytool", "-keystore", truststore_file, "-alias", "CARoot", "-import", "-file", crt_file,
        "-storepass", "testpassword", "-noprompt"
    ], check=True)

    subprocess.run([
        "keytool", "-keystore", keystore_file, "-alias", "localhost", "-import", "-file", crt_file,
        "-storepass", "testpassword", "-keypass", "testpassword", "-noprompt"
    ], check=True)

def generate_haproxy_config(num_kafka_nodes):
    haproxy_config = """
global
    log stdout format raw local0
    stats socket /tmp/haproxy.sock mode 660 level admin expose-fd listeners
    stats timeout 30s
    maxconn 4096
    user haproxy
    group haproxy
    daemon

defaults
    log global
    option tcplog
    timeout connect 5000ms
    timeout client  50000ms
    timeout server  50000ms

frontend kafka_tcp
    bind :80
    mode tcp
    default_backend kafka_backend

backend kafka_backend
    mode tcp
    balance roundrobin
"""

    for i in range(1, num_kafka_nodes + 1):
        haproxy_config += f"    server kafka{i} kafka{i}:909{1 + i} check\n"

    haproxy_config += """
frontend prometheus
    bind :8405
    mode http
    http-request use-service prometheus-exporter
    no log
"""

    return haproxy_config

def generate_docker_compose(num_kafka_nodes):
    template = """
  kafka{index}:
    image: confluentinc/cp-kafka:7.3.2
    hostname: kafka{index}
    container_name: kafka{index}
    ports:
      - "{port}:{port}"
      - "2{port}:2{port}"
    environment:
      KAFKA_ADVERTISED_LISTENERS: INTERNAL://kafka{index}:1{port},EXTERNAL://${{DOCKER_HOST_IP:-127.0.0.1}}:{port},DOCKER://host.docker.internal:2{port}
      KAFKA_LISTENER_SECURITY_PROTOCOL_MAP: INTERNAL:PLAINTEXT,EXTERNAL:PLAINTEXT,DOCKER:PLAINTEXT
      KAFKA_INTER_BROKER_LISTENER_NAME: INTERNAL
      KAFKA_ZOOKEEPER_CONNECT: "zoo1:2181"
      KAFKA_BROKER_ID: {index}
      KAFKA_LOG4J_LOGGERS: "kafka.controller=INFO,kafka.producer.async.DefaultEventHandler=INFO,state.change.logger=INFO"
      KAFKA_AUTHORIZER_CLASS_NAME: kafka.security.authorizer.AclAuthorizer
      KAFKA_ALLOW_EVERYONE_IF_NO_ACL_FOUND: "true"
    depends_on:
      - zoo1
"""

    docker_compose = """services:
  zoo1:
    image: confluentinc/cp-zookeeper:7.3.2
    hostname: zoo1
    container_name: zoo1
    ports:
      - "2181:2181"
    environment:
      ZOOKEEPER_CLIENT_PORT: 2181
      ZOOKEEPER_SERVER_ID: 1
      ZOOKEEPER_SERVERS: zoo1:2888:3888
"""

    haproxy = """
  haproxy:
    image: haproxy:2.9
    container_name: haproxy
    ports:
      - "80:80"
      - "8405:8405"
      - "443:443"
    ulimits:
      nofile:
        soft: 10000
        hard: 30000
    volumes:
      - ./haproxy.cfg:/usr/local/etc/haproxy/haproxy.cfg
      - ./certs/haproxy:/etc/haproxy/certs
    depends_on:
"""

    for i in range(1, num_kafka_nodes + 1):
        kafka_service = template.format(index=i, port=9092 + (i - 1))
        docker_compose += kafka_service

    docker_compose += haproxy
    for i in range(1, num_kafka_nodes + 1):
        docker_compose += f"      - kafka{i}\n"

    return docker_compose

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python generate_docker_compose.py <num_kafka_nodes>")
        sys.exit(1)

    num_kafka_nodes = int(sys.argv[1])
    docker_compose_content = generate_docker_compose(num_kafka_nodes)

    with open("docker-compose.yml", "w") as f:
        f.write(docker_compose_content)

    print(f"Docker Compose file for {num_kafka_nodes} Kafka nodes generated successfully.")

    haproxy_config_content = generate_haproxy_config(num_kafka_nodes)

    with open("haproxy.cfg", "w") as f:
        f.write(haproxy_config_content)

    print(f"HAProxy configuration file for {num_kafka_nodes} Kafka nodes generated successfully.")

    # update domain if not accessing from localhost...
    generate_ssl_certificates("certs", "localhost")