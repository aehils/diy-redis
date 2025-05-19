# DIY Redis: High Performance In-Memory Data Store in C/C++

This is an implementation of a Redis-like server from scratch in C++. It is an in-memory data structure store that can be used as a database, cache, message broker, or streaming engine. My implementation focuses on the core networking, infrastructure and data storage aspects of Redis. It includes:

1. A TCP server that can handle multiple concurrent client connections
2. A non-blocking I/O model with an event loop
3. A custom binary protocol for client-server communication
4. Basic key-value operations (GET, SET, DEL)
5. A simple client implementation for testing

Redis is one of the most widely-used pieces of infrastructure software today as it turns out, powering systems important to folks like Twitter, GitHub, Snapchat, StackOverflow, etc. It's open source, it's fast, and it's versatile.

## Architecture

This project consists of two main components:

### TCP Server (`tcp_serv.cpp`)

The server component implements:
- Socket creation and connection handling
- Non-blocking I/O operations
- An event loop using the `poll()` syscall
- Request parsing and response generation
- In-memory key-value storage using a C++ STL map

### TCP Client (`tcp_client.cpp`)

The client component implements:
- Socket connection to the server
- Message serialization according to the custom protocol
- Command-line interface for sending commands to the server

## Protocol

The project uses a custom binary protocol for communication:

**Request Format:**
```
[length prefix (4 bytes)][number of strings (4 bytes)][string1 length (4 bytes)][string1 data][string2 length (4 bytes)][string2 data]...
```

**Response Format:**
```
[length prefix (4 bytes)][status code (4 bytes)][response data]
```

## Supported Commands

The server currently supports the following commands:

1. `GET key` - Retrieve the value associated with the given key
2. `SET key value` - Set the value for the given key
3. `DEL key` - Delete the key-value pair for the given key
4. `close` - Close the connection explicitly if you so wish (though not very useful in its current state)

## Implementation Details

### Non-blocking I/O

The server uses non-blocking I/O to handle multiple client connections concurrently without using threads. Key components include:

- `set_nonblocking()` - my helper function to set socket file descriptors to non-blocking mode
- `poll()` system call to wait for socket events
- Buffer management for partial reads and writes

### Connection Management

The server maintains a map of client connections, each represented by a `Connected` struct that contains:
- File descriptor for the client socket
- Flags indicating whether the connection wants to read, write, or close
- Buffers for incoming and outgoing data

### Request Processing

When data is received from a client:
1. It's appended to the client's incoming buffer
2. The server attempts to parse complete requests from the buffer
3. Each request is processed by the appropriate command handler
4. The response is generated and added to the client's outgoing buffer
5. The server switches to write mode to send the response back to the client

## Building and Running

### Prerequisites

- A C++ compiler with C++11 support
- POSIX-compliant system (Linux, macOS, etc.)

### Building

```bash
# Compile the server
g++ -std=c++11 tcp_serv.cpp -o server

# Compile the client
g++ -std=c++11 tcp_client.cpp -o client
```

### Running

Start the server:
```bash
./server
```

Use the client to send commands:
```bash
# Set a key
./client set "crimson cipher" "decoded by those who dance with their own shadows"

# Get a key
./client get "crimson cipher"

# Delete a key
./client del "crimson cipher"
```

## Design Choices

1. **Non-blocking I/O**: The server uses non-blocking I/O to handle multiple clients concurrently without threads, which is more efficient for this use case.

2. **Binary Protocol**: A binary protocol was chosen over a text-based one for efficiency and to match Redis's approach.

3. **In-memory Storage**: Data is stored in memory using a C++ `std::map`, similar to Redis's primary storage mechanism.

4. **Error Handling**: The server includes comprehensive error handling for socket operations and protocol parsing.

## Limitations

Compared to a full Redis implementation, my Redis has several limitations in its current state:

1. Only supports basic key-value operations (GET, SET, DEL)
2. No persistence mechanisms
3. No data types beyond strings
4. No authentication or access control
5. No clustering or replication capabilities

## Learning Objectives

My work here demonstrates several important concepts in systems programming:

1. Socket programming and TCP networking
2. Non-blocking I/O and event loops
3. Binary protocol design and implementation
4. Buffer management and parsing
5. Concurrent server design patterns

## Future Enhancements

I have plans to make enhancements to this program. They will include, but perhaps not be limited to:

1. Adding more data types (lists, sets, hashes)
2. **Serialization / Persistence**  
   Implement RDB/AOF-style snapshotting and append-only file logging to disk for data durability.  
3. **AVL Tree / Sorted Sets**  
   Add support for sorted sets via an AVL tree (or skiplist) to enable range queries and ranking operations.  
4. **Timers & Key Expiry**  
   Integrate a timer system (e.g. using a min-heap) for efficient key expiration and timeouts.  
5. **Binary Heap for Eviction**  
   Use a binary heap to manage eviction policies (LRU, LFU) and fast access to the “next-to-evict” key.  
6. **Threaded I/O & Sharding**  
   Introduce threading (or async worker pools) to parallelize I/O and command processing, plus data sharding for horizontal scaling.  

## Acknowledgements

This project was inspired by the "Build Your Own Redis" tutorial series at https://build-your-own.org/redis/. The implementation follows the guidance provided in the tutorial but with custom modifications and enhancements.


