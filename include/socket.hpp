#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <expected>

struct SocketError{
    int  err;
    std::string message;
};

class Socket{
    public:
        Socket() = default;
        explicit Socket(int fd) noexcept : fd_(fd){}
        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator = (const Socket&) =delete;

        Socket (Socket&& other) noexcept;
        Socket& operator = (Socket&& other) noexcept;

        int fd() const noexcept{ return fd_; }
        bool valid() const noexcept { return fd_ >= 0; }

        static std::expected < Socket, SocketError> create_tcp();
        std::expected <void, SocketError> bind_and_listen(uint16_t port , int backlog = 128);
        std::expected < Socket, SocketError> accept();
        std::expected < void, SocketError> set_nonblocking();
        std::expected <size_t, SocketError>read_some(uint8_t* buf, size_t len);
        std::expected <size_t, SocketError>write_some(const uint8_t* buf, size_t len);

    private:
        int fd_ = -1;
};

