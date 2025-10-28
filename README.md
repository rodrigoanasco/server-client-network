Simple Group Chat Server with Fuzzing Clients

A lightweight, concurrent group chat system implemented in C++, featuring both a multi-client server and a fuzzing client generator. 
This project demonstrates network programming, concurrency, and protocol design using low-level sockets and threading primitives.

Based on:
Title: Assignment 11: A Simple Group Chat Server with Fuzzing Clients from CMPT201 Systems Programming
Link: https://github.com/SFU-CMPT-201/a11-Roddro1


Overview
---------
The system consists of:
- A Group Chat Server that manages multiple client connections and ensures message ordering consistency.
- A Fuzzing Client that automatically generates random test messages, sends them to the server, and logs the communication.

Both programs follow a custom messaging protocol with well-defined message types and a graceful termination sequence based on a two-phase commit protocol.

Features
---------
Group Chat Server
- Supports multiple simultaneous client connections using AF_INET sockets.
- Broadcasts every received message to all clients, including the sender.
- Ensures global message ordering across all clients.
- Includes client identification via IP and port embedding within each message.
- Gracefully handles session termination through a two-phase commit mechanism.
- Implemented with std::thread, std::mutex, and safe concurrent data structures in C++17.

Command:
./server <port_number> <#_of_clients>

Fuzzing Client
- Connects to the server and sends randomly generated messages using a fuzzing technique based on getentropy().
- Implements concurrent send and receive threads to simulate realistic chat behavior.
- Follows the custom binary message protocol:
  - type (uint8_t) — indicates message type (0 for chat, 1 for termination)
  - IP (uint32_t) + Port (uint16_t) — added by the server for identification
  - Message content (<= 1024 bytes), ending with '\n'
- Logs all received messages to a specified file path with IP and port formatting.

Command:
./client <IP_address> <port_number> <#_of_messages> <log_file_path>


How to Run
-----------
1. Build the project with CMake:
   $ mkdir build && cd build
   $ cmake ..
   $ cmake --build . -j

2. Start the server:
   $ ./server 8080 3

3. Open three terminals and run the clients:
   $ ./client 127.0.0.1 8080 5 logs/client1.txt
   $ ./client 127.0.0.1 8080 5 logs/client2.txt
   $ ./client 127.0.0.1 8080 5 logs/client3.txt

4. Wait for all clients to finish.
   Each client sends 5 random fuzzed messages, then sends a type 1 termination message.
   Once all clients are done, the server broadcasts a final termination signal and exits gracefully.


Output Explanation
-------------------
Server Output:
---------------
When you run the server, you’ll see:
   Server listening on port 8080 expecting 3 clients

As clients connect and send data, the server silently broadcasts messages.
After all clients finish sending, the server sends a type 1 “END” message to all clients and terminates.

Client Output:
--------------
Each client prints all messages it receives from the server. Each line contains:
   <IP>  <Port>  <Hex Message>

Example:
   127.0.0.1       8080      4F3D9C1E2B8A19
   127.0.0.1       8080      13ACF2E59304D0

- IP Address: Sender’s IPv4 address
- Port: Sender’s TCP port
- Message: Random hexadecimal payload

When the client receives the type 1 END message, it stops automatically.

Client Logs:
------------
Each client saves the same messages to its log file (e.g., client1.txt):
   127.0.0.1       8080      4F3D9C1E2B8A19
   127.0.0.1       8080      13ACF2E59304D0

All clients should contain identical log content in the same order, ensuring that message ordering is consistent.


Messaging Protocol Summary
---------------------------
Field       | Type      | Description
------------|-----------|------------------------------------------
type        | uint8_t   | 0 = regular message, 1 = termination signal
ip          | uint32_t  | Sender's IP address (added by server)
port        | uint16_t  | Sender's port (added by server)
payload     | char[]    | Random or chat message
terminator  | '\n'      | Marks the end of the message


Two-Phase Commit Termination
----------------------------
1. Each client sends a type 1 message after finishing its assigned messages.
2. The server waits until it receives a type 1 message from all clients.
3. Once all are done, the server broadcasts a final type 1 message back to every client.
4. All clients and the server terminate cleanly.

This ensures deterministic and consistent termination across distributed participants.


Project Structure
-----------------
```
GroupChat/
│
├── include/
│   ├── protocol.hpp
│
├── src/
│   ├── server.cpp
│   └── client.cpp
│
└── CMakeLists.txt
```

Technologies & Concepts
-----------------------
- Language: C++17
- Networking: BSD Sockets (AF_INET, TCP)
- Concurrency: std::thread, std::mutex, condition_variable
- Fuzzing: Random byte generation via getentropy()
- I/O: Concurrent read/write streams
- Protocol Design: Binary framing with address embedding
- Testing: Manual telnet testing + log verification


Example Usage
--------------
Start the server:
$ ./server 8080 3

Connect three clients:
$ ./client 127.0.0.1 8080 5 logs/client1.txt
$ ./client 127.0.0.1 8080 5 logs/client2.txt
$ ./client 127.0.0.1 8080 5 logs/client3.txt

Each client will automatically send fuzzed messages, receive all others’ messages, and log them in order.


Learning Outcomes
-----------------
- Socket-level communication and multiplexed I/O
- Synchronization across threads and distributed systems
- Designing robust custom protocols
- Graceful shutdown using commit-based coordination


Future Improvements
-------------------
- Migrate to epoll or select() for non-blocking I/O scalability.
- Add a real-time UI using ncurses or a simple web dashboard.
- Support encrypted communication (TLS) and user authentication.


Author
-------
Rodrigo Añasco <br/>
Computing Science Student, Simon Fraser University <br/>
Burnaby, BC <br/>
github.com/rodrigoanasco
