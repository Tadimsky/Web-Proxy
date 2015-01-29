cmake_minimum_required(VERSION 2.8.4)
project(Web_Proxy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    src/csapp.c
    src/csapp.h
    src/proxy.c)

find_package (Threads)

add_executable(Web_Proxy ${SOURCE_FILES})

target_link_libraries (Web_Proxy ${CMAKE_THREAD_LIBS_INIT})