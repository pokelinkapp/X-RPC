if("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(X_RPC_COMPILE_TYPE SHARED)
else()
    set(X_RPC_COMPILE_TYPE STATIC)
endif()

add_library(X-RPC ${X_RPC_COMPILE_TYPE} x_RPC.c)
target_link_libraries(X-RPC msgpackc)

if(CMAKE_HOST_WIN32)
    target_link_libraries(X-RPC ws2_32)
endif()

if("${OS}" STREQUAL "Win32")
    add_definitions(-DWIN32)
    target_link_libraries(X-RPC ws2_32)
endif()

if("${X-RPC_Test_enabled}" STREQUAL "ON")
    add_executable(X-RPC_test test/main.cpp)
    target_link_libraries(X-RPC_test X-RPC)

    if(CMAKE_HOST_UNIX)
        target_link_libraries(X-RPC_test pthread)
    endif()

    if(NOT CMAKE_HOST_WIN32)
        target_link_options(X-RPC_test PUBLIC -static -static-libgcc -static-libstdc++)
    endif()
endif()