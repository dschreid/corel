cmake_minimum_required(VERSION 3.16)
project(corel)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error")

add_executable(${PROJECT_NAME} src/main.c)
