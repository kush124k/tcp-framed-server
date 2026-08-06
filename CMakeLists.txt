cmake_minimum_required(VERSION 3.16)
project(tcp-framed-server CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)

add_executable(server src/main.cpp src/socket.cpp)
