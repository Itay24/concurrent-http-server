
# Concurrent HTTP Server in C

A multithreaded HTTP/1.x server built from scratch in C using Linux sockets and POSIX threads.

## Features

- TCP socket networking and HTTP request parsing
- Static HTML file serving
- Fixed-size worker thread pool
- Bounded, thread-safe connection queue
- Mutex and condition-variable synchronization
- Nonblocking connection acceptance using `poll()`
- Graceful shutdown using POSIX signals
- Request-size limits and socket receive timeouts
- Automated HTTP integration tests

## Architecture

The main thread accepts incoming connections and places them into a bounded queue. A fixed set of worker threads retrieves connections and handles HTTP requests independently.

The queue uses a mutex and condition variable to coordinate access without busy-waiting.

## Building

Requirements: Linux, GCC, Make, and POSIX threads.

```bash
make
./server
```

The server listens on `127.0.0.1:8080`.

## Testing

Start the server, then run:

```bash
python3 tests/test_server.py
```

The test suite covers successful requests, missing routes, unsupported methods, malformed requests, and concurrent clients.

## Future Work

- Performance benchmarking across concurrency models
- Additional robustness and security tests
- AI-assisted server performance investigation
