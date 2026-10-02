#include "socket.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>



Socket::Socket( Socket&& other ) noexcept : fd_( other.fd_ ){
    other.fd_ = -1;
}

Socket& Socket::operator = (Socket&& other ) noexcept {
    if( this != &other ) {
        if( fd_ >= 0 ){
            ::close( fd_ );
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }

    return *this;
    
}

//AF_INET refers to address family IPv4 and AF_INET6 is for IPv6
//SOCK_STREAM is a socket type which is reliable, ordered and connection based byte stream(making it TCP)
//SOCK_DGRAM would be socket type(UDP) with no connection, no ordering gaurantee and no automatic transmission

std::expected<Socket, SocketError> Socket::create_tcp() {
    int fd = ::socket(AF_INET,SOCK_STREAM,0); //actual syscall that creates a socket and returns a fd for it.
    if( fd<0 ){
        return std::unexpected(SocketError{errno, std::string("Socket() failed: ") + std::strerror(errno)});

    }
    
    int opt = 1;

    if(::setsockopt(fd,SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0){
        ::close(fd);
        return std::unexpected(SocketError{errno, std::string("setsockopt() failed ") + std::strerror(errno)});
    }

    return Socket(fd);
}

std::expected<void, SocketError> Socket::bind_and_listen(uint16_t port , int backlog){
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr*>( &addr ) , sizeof(addr)) < 0){
        return std::unexpected(SocketError{errno, std::string("bind() failed: ") + std::strerror(errno)});
    }

    if(::listen(fd_, backlog) < 0){
        return std::unexpected(SocketError{errno, std::string("listen() failed: ") + std::strerror(errno)});
    }


    return {};
}

std::expected<Socket, SocketError> Socket::accept(){
    int  client_fd = ::accept(fd_, nullptr, nullptr);
    if(client_fd<0){
        return std::unexpected(SocketError{errno, std::string("accept() failed: ") + std::strerror(errno)});
    }

    return Socket(client_fd);
}

std::expected<void, SocketError> Socket::set_nonblocking(){
    int read_flag = ::fcntl(fd_, F_GETFL, 0);
    if(read_flag<0){
        return std::unexpected(SocketError{errno, std::string("set_nonblocking() getfl failed: ") + std::strerror(errno)});
    }

    int write_fd = ::fcntl(fd_, F_SETFL, read_flag | O_NONBLOCK);
    if(write_fd<0){
        return std::unexpected(SocketError{errno, std::string("set_nonblocking() setfl failed: ") + std::strerror(errno)});
    }

    return {};

}

std::expected<size_t, SocketError> Socket::read_some( uint8_t* buf, size_t len){
    ssize_t readt = ::recv(fd_, buf, len, 0);
    if( readt < 0 ){
        return std::unexpected(SocketError{errno, std::string("recv() failed : ") + std::strerror(errno)});
    }

    return static_cast<size_t>(readt);
}

std::expected<size_t, SocketError> Socket::write_some( const uint8_t* buf, size_t len){
    ssize_t sent = ::send(fd_,buf,len, 0);

    if( sent<0 ){
        return std::unexpected(SocketError{errno, std::string("send() failed : ") + std::strerror(errno)});
    }

    return static_cast<size_t>(sent);

}
