# tcp-framed-server

A TCP server built from raw POSIX sockets and `epoll` — no Boost.Asio, no
libuv. Implements a length-prefixed message framing protocol on top of
TCP's raw byte stream, with a working echo server as the reference
application.

Built as a from-scratch systems programming exercise: every syscall
(`socket`, `bind`, `listen`, `accept`, `epoll_ctl`, `fcntl`, `recv`,
`send`) is called directly, wrapped in a small RAII layer.

## Why this exists

TCP guarantees ordered, reliable delivery of bytes — it does **not**
guarantee that one `send()` on the client lines up with one `recv()` on
the server. A message can arrive split across multiple reads, or several
messages can arrive concatenated in a single read. Every real network
protocol (HTTP/2, gRPC, Kafka's wire protocol, Redis's RESP) solves this
exact problem. This project implements the simplest correct solution —
length-prefix framing — and proves it handles the split-message case
correctly, not just the lucky one-shot case.

## Wire protocol

```
[4-byte big-endian length][payload bytes]
```

The server accumulates incoming bytes per connection and extracts
complete frames as they become available, correctly handling frames that
arrive split across multiple `recv()` calls. Each complete frame is
echoed back to the sender in the same format.

## Architecture

- **`Socket`** (`include/socket.hpp`, `src/socket.cpp`) — a move-only
  RAII wrapper around a raw file descriptor. Copying is disabled at
  compile time to prevent double-close bugs; moving transfers ownership
  and invalidates the source. All fallible operations return
  `std::expected<T, std::string>` instead of throwing, keeping error
  handling explicit on the hot path.

- **Event loop** (`src/main.cpp`) — a single-threaded, non-blocking
  `epoll` loop. One listener socket accepts new connections; each client
  is tracked in a `ClientState` (socket + per-connection read buffer) so
  partial frames can accumulate correctly across multiple wakeups.

## Build

Requires a C++23 compiler (`std::expected`) and CMake 3.16+.

```
cmake -S . -B build
cmake --build build
./build/server <port> (or use basic 9000)
```

## Testing

A basic connection can be tested with `nc`, but proving the framing
logic works requires sending a message's length prefix and payload as
two separate writes with a delay between them — something `nc` alone
can't do on a held-open connection. `test_client.py` does exactly this:

```
python3 test_client.py
```

It opens a connection, sends the 4-byte length prefix, sleeps briefly,
then sends the payload separately, and prints the echoed response. The
server correctly waits for the full frame before processing it,
regardless of how the bytes were split across reads.

## What this doesn't do (yet)

- No handling of `EAGAIN`/`EWOULDBLOCK` as a normal non-blocking
  condition — currently any failed read/write disconnects the client
- No `EPOLLOUT`-driven writes — assumes the send buffer always has room,
  so a very slow client under load could see a partial write mishandled
- No graceful shutdown on `SIGINT` — relies on the OS to clean up fds on
  process exit
- Single-threaded — no `SO_REUSEPORT` or multi-threaded accept

These are known, deliberate scope cuts, not oversights.