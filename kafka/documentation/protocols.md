# Intro

The following messages are sorted by entity **sending** them.
The following syntax is used:

- `bytes(type x)`: the serialization of x in bytes. The type of x is annotated.
For example, `bytes(int 1)` is equivalent to `\0x00\0x00\0x00\0x01`
- `bytes(size_t n)[type data]` a list of data of a given type. The number of element in the list must always be written before the list.
- `sep` is `\r\n`, which is just two bytes `\0x0d\x0a`
- `opcode` is the opcode of the operation

All data is passed in little endian.
Be careful when transforming strings into bytes: ip addresses are strings of 15 bytes, followed by an integer of 4 bytes representing the port. If the length of the ip address' string is less than 15, you must right fill it with `\0`.

# MASTER

## 0 HELLO RL

Master sends this message to RL so that RL knows the IP and port of master.

**format** `opcode bytes(int 0) sep`

## 1 RL IP
Master sends this message to all server nodes in response to them pinging it (see **200 ready**). It contains the ip and port of the RL.

**format** `opcode bytes(char[15] ip) bytes(int port) sep`

## 2 Authority transfer SN

Master sends this message to nodes that have to change a player from simulated to replicated, and from replicated to simulated. The two nodes implicated in the switch must receive this message.

**format** `opcode bytes(int clientId) bytes(bool drop) bytes(char[15] ip) bytes(int port) sep`

The `drop` boolean must be set to true if the node is currently simulating the player and should stop doing so, and to false otherwise.

## 3 Authority transfer Client

Indicates to a client that it should change the server it is connected to.

**format** `opcode bytes(char[15] ip) bytes(int port) sep`

## 5 Goto

This message is sent in response to `300 Hello Master`.
It indicates to the client to which server node to connect to.

**format** `opcode bytes(char[15] ip) bytes(int port) bytes(int clientId) sep`

# RL

## 101 GameState to master

**format** `opcode bytes(size_t nbPlayers) [bytes(int playerId) bytes(float x) bytes(float y)] sep`

## 102 Replicate simulation to sn

**format** `opcode bytes(size_t nbPlayers)[bytes(int playerId) bytes(SNClientState state)] sep`

# SN

## 200 Ready

Indicates to the master that the node is up and running.

**format** `opcode sep`

## 201 GameState

Updates a client telling it what is the state of the game.

**format** `opcode bytes(size_t nbPlayers)[bytes(int playerId) bytes(SNClientState state)] sep`

## 202 Simulation Results

Sends the computed simulation results to the replication layer.

**format** `opcode bytes(size_t nbPlayers)[bytes(int playerId) bytes(SNClientState state)] sep`

## 203 Hello Repl

The server node authenticates with the RL.

**format** `opcode sep`


# Client

## 300 Hello master

Pings the master to connect to it.

**format** `opcode sep`
 The master should respond with `5 Goto`.

## 302 Hello sn

Connects to the SN.

**format** `opcode bytes(int clientId) sep`

## 310 - 317 Inputs

The client sends its inputs to the SN.

**format** `opcode bytes(int clientId) bytes(float[3] forwardVector) bytes(float[3] rightVector) sep`
q