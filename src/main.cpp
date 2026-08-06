#include "socket.hpp"
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <arpa/inet.h>


// struct epoll_event {  this is what epoll predefined struct looks like
//     uint32_t events;
//     epoll_data_t data;
// }

struct ClientState{
    Socket socket;
    std::vector<uint8_t> read_buf;
};

int main(int argc, char** argv){

    std::vector<epoll_event> events(64);
    std::unordered_map<int, ClientState> clients;

    if(argc !=2){
        std::fprintf(stderr, "usage %s <port> \n", argv[0]);
        return 1;
    }
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));

    auto listener = Socket::create_tcp();

    if(!listener){
        std::fprintf(stderr, "%s \n", listener.error().c_str());
        return 1;
    }

    if(auto result = listener->bind_and_listen(port); !result){
        std::fprintf(stderr, "%s \n", result.error().c_str());
        return 1;
    }

    if(auto result = listener->set_nonblocking(); !result){
        std::fprintf(stderr, "%s\n", result.error().c_str());
        return 1;
    }

    std::printf("listening on port %u \n", port);

    int epfd =  epoll_create1(0);
    if(epfd <0){
        std::fprintf(stderr, "epoll_create1 failed : \n" );
        return 1;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listener->fd();

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, listener->fd(), &ev) <0){
        std::fprintf(stderr, "epoll_ctl failed \n");
        return 1;
    }

    std::printf("registered listener with epoll \n");

    while(true){
        int n = epoll_wait(epfd, events.data(), events.size(), -1);
        if(n<0){
            std::perror("epoll_wait");
            break;
        }

        for(int i=0 ; i<n ; ++i){
            int fd = events[i].data.fd;

            if(events[i].data.fd == listener->fd()){
                auto client = listener->accept();
                if(!client){
                    std::fprintf(stderr, "%s \n", client.error().c_str());
                    continue;
                }

                int client_fd =client->fd();

                if(auto result = client->set_nonblocking(); !result){
                    std::fprintf(stderr, "%s\n", result.error().c_str());
                    continue;
                }
                epoll_event client_ev{};
                client_ev.events = EPOLLIN;
                client_ev.data.fd = client_fd;

                if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0){
                    std::fprintf(stderr, "epoll_ctl(client) failed\n");
                    continue;
                }

                clients.emplace(client_fd, ClientState{std::move(*client), {}});
                std::printf("accepted client. fd=%d \n", client_fd);
            }else{
                auto it = clients.find(fd);
                if(it == clients.end()) 
                continue;

                uint8_t buf[1024];
                auto result = it->second.socket.read_some(buf, sizeof(buf));

                if(!result || *result == 0){
                    std::printf("client fd=%d disconnected \n", fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    clients.erase(it);
                    continue;
                }

                auto& read_buf = it->second.read_buf;
                read_buf.insert(read_buf.end(), buf, buf + *result);

                while(true){
                    if(read_buf.size() < 4) break;

                    uint32_t length;
                    std::memcpy(&length, read_buf.data(), 4);
                    length = ntohl(length);

                    if(read_buf.size() < 4 + length) break;

                    std::vector<uint8_t> payload(read_buf.begin() + 4, read_buf.begin() + 4 + length);
                    read_buf.erase(read_buf.begin(), read_buf.begin() + 4 + length);

                    std::printf("fd=%d complete frame, %ubytes: %.*s\n", fd, length, (int)length, payload.data());

                    uint32_t net_length = htonl(length);
                    it->second.socket.write_some(reinterpret_cast<uint8_t*>(&net_length), 4);
                    it->second.socket.write_some(payload.data(), payload.size());
                }
            }
        }
    }

    return 0;
}