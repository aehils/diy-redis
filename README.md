# DIY Redis: High-Performance In-Memory Data Store in C/C++

This is my implementation of a Redis from scratch in C++. Redis is an in-memory data structure store that can be used as a database, cache, message broker, or streaming engine. I've focused on the core networking, infrastructure and data storage aspects of Redis. That includes:

1. A TCP server that can handle concurrent client connections
2. A non-blocking I/O model with an event loop
3. A custom binary protocol for client-server communication
4. Basic key-value operations (GET, SET, DEL, UPDATE)
5. A simple client implementation for testing

Redis is one of the most widely-used pieces of infrastructure software today, powering systems important to folks like Twitter, GitHub, Snapchat, StackOverflow, etc. It's open source, it's fast, and it's versatile.

## Architecture

Two main components:

### TCP Server (`tcp_serv.cpp`)

The server component implements:
- Sockets and connection handling
- Non-blocking I/O operations
- An event loop using the `poll()` syscall
- Request parsing and response generation
- In-memory key-value storage using a C++ STL map (for now)

### TCP Client (`tcp_client.cpp`)

The client component implements:
- Socket connection to the server
- Message serialisation according to the custom protocol
- CLI for sending commands to the server

## Protocol

I'm using a custom wire protocol for communication:

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

The server uses non-blocking I/O to handle multiple client connections concurrently without using threads. Note:

- `set_nonblocking()` - my helper function to set socket file descriptors to non-blocking mode
- `poll()` system call to wait for socket events
- Buffer management for partial reads and writes

## Building and Running

### Prerequisites

- A C++ compiler with C++11 support
- POSIX-compliant system (Linux, macOS, etc.)

### Building

You can clone the repo or download the zip if you also want to try this out in its current state. You can store any data structure in your fast-access memory, which of course means that data is quickly retrivable. Perhaps use as a caching server on your Pi.

```bash
# Compile the server
g++ tcp_serv.cpp -o server

# Compile the client
g++ tcp_client.cpp -o client
```

### Running

Start the server and/or the client after compiling:
```bash
./server
```
```bash
./client
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

## Limitations

Compared to a full Redis implementation, my Redis has several limitations in its current state:

1. Only supports a few basic key-value operations (GET, SET, DEL)
2. No persistence yet
3. No data types beyond strings
4. No authentication or access control
5. No clustering or replication capabilities

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

This project was inspired by the "Build Your Own Redis" tutorial series at https://build-your-own.org/redis/. My implementation follows the guidance provided in the tutorial but with my own spin on things. Please enjoy! Feedback is always welcome.


