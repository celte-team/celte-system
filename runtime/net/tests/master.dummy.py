#!/usr/bin/env python
import threading
import pulsar
import json

client = pulsar.Client('pulsar://localhost:6650')

nodes = []
# struct RPRequest : public CelteRequest<RPRequest> {
#   std::string name;       // the name of the rpc to invoke
#   std::string respondsTo; // left empty for a call, set to the id of the rpc for
#                           // a response of a previously called rpc
#   std::string responseTopic; // where to send the response
#   std::string rpcId;         // unique id for this rpc
#   nlohmann::json args;       // arguments to the rpc

#   void to_json(nlohmann::json &j) const {
#     j = nlohmann::json{{"name", name},
#                        {"respondsTo", respondsTo},
#                        {"responseTopic", responseTopic},
#                        {"rpcId", rpcId},
#                        {"args", args}};
#   }

#   void from_json(const nlohmann::json &j) {
#     j.at("name").get_to(name);
#     j.at("respondsTo").get_to(respondsTo);
#     j.at("responseTopic").get_to(responseTopic);
#     j.at("rpcId").get_to(rpcId);
#     j.at("args").get_to(args);
#   }
# };

def call_rpc(topic, name, args: tuple):
    request = {
        "name": name,
        "respondsTo": "",
        "responseTopic": "",
        "rpcId": "1",
        "args": [arg for arg in args]
    }
    global client
    producer = client.create_producer(topic)
    producer.send(json.dumps(request).encode('utf-8'))
    print(f"<Sent RPC request> {request} on topic {topic}")
# wait for the message to be sent
    producer.flush()
    producer.close()




def rpc_thread(master_rpc):
    while True:
        msg = master_rpc.receive()
        data = msg.data().decode('utf-8')
        json_data = json.loads(data)

        # Extract fields from the JSON data
        headers = msg.properties()  # Get custom headers

        # Display the data
        print(f"Received message: id={id}, message={json_data}, headers={headers}")

        master_rpc.acknowledge(msg)


def hello_sn_thread(master_hello_sn):
    while True:
        msg = master_hello_sn.receive()
        data = msg.data().decode('utf-8')
        json_data = json.loads(data)

        # Extract fields from the JSON data
        headers = msg.properties()  # Get custom headers

        # Display the data
        print(f"Received message: id={id}, message={json_data}, headers={headers}")

        global nodes
        nodes.append(json_data['peerUuid'])
        print(f"<Registered node> {json_data['peerUuid']}")

        master_hello_sn.acknowledge(msg)

        call_rpc(f"persistent://public/default/{json_data['peerUuid']}.rpc", "__rp_assignGrape", ["LeChateauDuMechant"])


def hello_client_thread(master_hello_client):
    while True:
        msg = master_hello_client.receive()
        data = msg.data().decode('utf-8')
        json_data = json.loads(data)

        # Extract fields from the JSON data
        headers = msg.properties()  # Get custom headers

        # Display the data
        print(f"Received message: id={id}, message={json_data}, headers={headers}")

        master_hello_client.acknowledge(msg)


# master_rpc = client.subscribe('persistent://public/default/master.rpc', subscription_name='my-sub')
master_hello_sn = client.subscribe('persistent://public/default/master.hello.sn', subscription_name='my-sub')
master_hello_client = client.subscribe('persistent://public/default/master.hello.client', subscription_name='my-sub')

# one thread per topic
hello_client_thread = threading.Thread(target=hello_client_thread, args=(master_hello_client,))
hello_client_thread.start()

hello_sn_thread = threading.Thread(target=hello_sn_thread, args=(master_hello_sn,))
hello_sn_thread.start()

# wait for the threads to finish
hello_client_thread.join()
hello_sn_thread.join()