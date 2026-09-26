# tcp-framed-server

A raw TCP server built entirely from scratch using POSIX sockets and `epoll` (no external network libraries like Boost.Asio or libuv).

The primary goal of this project is to serve as an educational reference and mental model for how real-world network servers handle TCP's raw byte stream. It demonstrates the fundamental architectural patterns required to reliably handle network connections at scale, acting as a technical deep-dive into systems programming.

## Architectural Choices & Core Features

This project was built deliberately to showcase the underlying mechanics of a TCP server. Below are the key architectural choices, what they are, why they were chosen, and their pros.

### 1. Raw POSIX Sockets & `epoll`
**What it is:** Instead of relying on heavy abstractions, every syscall (`socket`, `bind`, `listen`, `accept`, `epoll_ctl`, `fcntl`, `recv`, `send`) is called directly. `epoll` is Linux's highly efficient I/O event notification facility.

**Why it was chosen:** To teach the realities of non-blocking I/O multiplexing. A single thread can manage thousands of concurrent connections by asking the kernel "which sockets are ready to read or write?" rather than spawning a heavy OS thread per connection.

**Pros:**
- **High Performance:** Minimal overhead compared to thread-per-connection models.
- **Deep Understanding:** Forces the developer to handle the actual mechanics of socket states rather than hiding them behind library abstractions.
- **Scalability:** `epoll` operates in O(1) time complexity regarding the number of active connections, unlike older `select` or `poll` models.

### 2. Length-Prefixed Framing (Solving the TCP Streaming Problem)
**What it is:** TCP is a continuous byte stream; it guarantees ordered delivery, but it does *not* guarantee message boundaries. A `send()` on the client might arrive split across multiple `recv()` calls, or several `send()`s might arrive concatenated. This server implements a wire protocol (`[4-byte big-endian length][payload bytes]`) to reassemble discrete messages.

**Why it was chosen:** Every real network protocol (HTTP/2, gRPC, Redis's RESP, Kafka's wire protocol) must solve this exact problem. Length-prefix framing is the simplest and most robust way to ensure we know exactly when a full message has been received.

**Pros:**
- **Correctness under load:** Proves the server can handle split-message cases, not just lucky one-shot deliveries.
- **Simplicity:** A 4-byte header is trivial to parse and requires minimal state tracking compared to delimiter-based protocols (like `\r\n` in HTTP/1.1) which require scanning every byte.

### 3. Per-Connection State & Buffering
**What it is:** Each connected client is tracked in a dedicated `ClientState` structure containing its socket descriptor and a dedicated `std::vector<uint8_t>` read buffer.

**Why it was chosen:** In a non-blocking event loop, a partial message might be read before the rest arrives. The server cannot block and wait for the rest of the bytes. Instead, it must save the partial bytes in a buffer, return control to the `epoll` loop to handle other clients, and resume processing when more bytes arrive.

**Pros:**
- **No Blocking:** Prevents a slow client from monopolizing the single server thread.
- **Reliable Reassembly:** Allows the framing logic to cleanly accumulate incoming bytes over multiple wakeups and safely extract complete frames as they become available.

### 4. Move-Only RAII File Descriptor Management
**What it is:** The raw socket file descriptors are wrapped in a `Socket` C++ class utilizing RAII (Resource Acquisition Is Initialization). Copying the class is explicitly disabled at compile time; it can only be moved.

**Why it was chosen:** Raw integer file descriptors are notorious for leaking or being double-closed, causing severe bugs (e.g., closing an FD that the OS has already reassigned to another connection).

**Pros:**
- **Safety:** The compiler guarantees that a socket is closed exactly once when its wrapper goes out of scope.
- **Ownership Semantics:** Moving transfers ownership explicitly, invalidating the source wrapper and preventing accidental concurrent use.

### 5. Explicit Error Handling via C++23 `std::expected`
**What it is:** Fallible operations (like binding, listening, or accepting connections) return `std::expected<T, std::string>` instead of throwing C++ exceptions.

**Why it was chosen:** Exceptions in systems programming can obscure control flow and incur hidden performance costs. Network operations frequently fail (e.g., connection reset by peer), which is expected behavior, not an exceptional crash.

**Pros:**
- **Explicit Hot Path:** Forces the caller to explicitly handle success and failure paths at the call site.
- **Performance:** Avoids the overhead of stack unwinding associated with C++ exceptions.

## Build

Requires a C++23 compiler (`std::expected`) and CMake 3.16+.

```bash
cmake -S . -B build
cmake --build build
./build/server <port> # (or use basic 9000)
```

## Testing

A basic connection can be tested with `nc`, but proving the framing logic works requires sending a message's length prefix and payload as two separate writes with a delay between them — something `nc` alone can't do on a held-open connection. `test_client.py` does exactly this:

```bash
python3 test_client.py
```

It opens a connection, sends the 4-byte length prefix, sleeps briefly, then sends the payload separately, and prints the echoed response. The server correctly waits for the full frame before processing it, regardless of how the bytes were split across reads.

## What this doesn't do (yet)

- No handling of `EAGAIN`/`EWOULDBLOCK` as a normal non-blocking condition — currently any failed read/write disconnects the client.
- No `EPOLLOUT`-driven writes — assumes the send buffer always has room, so a very slow client under load could see a partial write mishandled.
- No graceful shutdown on `SIGINT` — relies on the OS to clean up fds on process exit.
- Single-threaded — no `SO_REUSEPORT` or multi-threaded accept.

These are known, deliberate scope cuts, not oversights.
