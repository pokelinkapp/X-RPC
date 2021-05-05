cmake_minimum_required(VERSION 3.14)
project(X_RPC)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)

include_directories(
    include
)

if("${XRPC_MINGW}" STREQUAL "ON")
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/X-RPC.dll" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/X-RPC.lib")
        message(FATAL_ERROR "X-RPC cannot compile under mingw. Please compile for windows and store it in the root folder")
    endif()
    add_library(X-RPC SHARED IMPORTED)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(X-RPC PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/X-RPC.dll"
        IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/X-RPC.lib")
    # include_directories(
    #     dependencies/msgpack-c/include
    # )
else()
    set(MSGPACK_BUILD_TESTS OFF)
    set(MSGPACK_BUILD_EXAMPLES OFF)

    if("${BUILD_SHARED_LIBS}" STREQUAL "ON")
        add_compile_definitions(X_RPC_STANDALONE)
    endif()

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    if("${X-RPC_Test_enabled}" STREQUAL "")
        set(X-RPC_Test_enabled ON)
        set(BUILD_SHARED_LIBS "OFF")
    endif()

    add_subdirectory(dependencies/msgpack-c)

    add_subdirectory(src)
endif()
