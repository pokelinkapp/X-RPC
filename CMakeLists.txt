cmake_minimum_required(VERSION 3.17)
project(X_RPC)

set(CMAKE_C_STANDARD 98)
set(CMAKE_CXX_STANDARD 17)

set(MSGPACK_BUILD_TESTS OFF)
set(MSGPACK_BUILD_EXAMPLES OFF)

if("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    add_compile_definitions(X_RPC_STANDALONE)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT X-RPC_Test_enabled)
    set(X-RPC_Test_enabled ON)
    set(BUILD_SHARED_LIBS "OFF")
endif()

include_directories(
        include
)

add_subdirectory(dependencies/msgpack-c)

add_subdirectory(src)