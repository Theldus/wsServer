cmake_minimum_required(VERSION 3.0.2)

project(ws C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

add_library(ws
    src/ws.c
    src/base64/base64.c
    src/sha1/sha1.c
    src/handshake/handshake.c)
target_link_libraries(ws pthread)

add_executable(send_receive
    example/send_receive.c)
target_link_libraries(send_receive ws)
