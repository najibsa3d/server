cmake_minimum_required(VERSION 3.20)
project(OS_hw3)

set(CMAKE_CXX_STANDARD 11)

add_executable(OS_hw3 main.cpp server.c segel.c request.c client.c output.c queue.c queue.h)
