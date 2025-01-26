cmake_minimum_required(VERSION 3.10)
project(corel C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LIBGIT2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/libgit2/include")
set(LIBGIT2_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/libs/libgit2/libgit2.a")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    # Remove -fsanitize=address from C flags
    string(REPLACE "-fsanitize=address" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

    # (Optional) Remove -fsanitize=address from C++ flags
    # string(REPLACE "-fsanitize=address" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SSH2 REQUIRED IMPORTED_TARGET libssh2 openssl libssl libcrypto zlib libpcre)

# Add OpenSSL library search paths
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(${PROJECT_NAME} src/main.c)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${LIBGIT2_INCLUDE_DIR}"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        "${LIBGIT2_LIBRARY}"
        ${OPENSSL_LIBRARIES}   # Link OpenSSL libraries
        PkgConfig::SSH2
        util
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/build/compile_commands.json ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
)

