cmake_minimum_required(VERSION 3.10)
project(corel C)

set(LIBGIT2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libgit2")
set(LIBGIT2_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libgit2/build")
set(LIBGIT2_INCLUDE_DIR "${LIBGIT2_DIR}/include")
set(LIBGIT2_LIBRARY "${LIBGIT2_BUILD_DIR}/libgit2.so")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error")

add_executable(${PROJECT_NAME} src/main.c)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${LIBGIT2_INCLUDE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        "${LIBGIT2_LIBRARY}"
)

