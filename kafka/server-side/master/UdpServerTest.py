import socket
import sys
import argparse
from time import sleep
import struct

def start_udp_server(host='127.0.0.1', port=13000, master=12000):
    # Create a UDP socket
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server_socket:
        # Bind the socket to the server address
        server_socket.bind((host, port))
        print(f"UDP server listening on {host}:{port}")

        # Send message to client when created
        # Convert integer 2 to 4 bytes
        integer_value = 2
        response_message = struct.pack('!I', integer_value)
        additional_message = "bonjour\r\n".encode('utf-8')
        final_message = response_message + additional_message
        server_socket.sendto(final_message, ('127.0.0.1', master))
        print(f"Sent acknowledgment to client: {response_message + additional_message}")
        count = 0
        while True:
            # Receive data from a client
            data, client_address = server_socket.recvfrom(1024)  # Buffer size is 1024 bytes
            print(f"Received message from {client_address}: {data.decode('utf-8')}")
            sleep(2)
            integer_value = 2
            # response_message = struct.pack('!I', integer_value)
            # additional_message = " bonjour\r\n".encode('utf-8')
            count += 1
            server_socket.sendto(final_message, ('127.0.0.1', master))
            print(f"Sent acknowledgment to client: {response_message + additional_message}, count = {count}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Start a UDP server.')
    parser.add_argument('--port', type=int, default=13000, help='Port to listen on (default: 13000)')
    parser.add_argument('--master', type=int, default=16110, help='Master port to send messages to (default: 16110)')
    args = parser.parse_args()

    start_udp_server(port=args.port, master=args.master)
