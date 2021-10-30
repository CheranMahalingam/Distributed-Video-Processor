cmake_minimum_required(VERSION 3.16)

project(DistributedVideoProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

add_executable(server src/tcp_server.cpp)
add_executable(client src/tcp_client.cpp)

option(USE_SYSTEM_BOOST "Use system installed boost" ON)
if(USE_SYSTEM_BOOST)
    find_package(Boost CONFIG REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost
    )
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(boost)
endif()

option(USE_SYSTEM_GRPC "Use system installed gRPC" OFF)
if(USE_SYSTEM_GRPC)
    find_package(gRPC CONFIG REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG        v1.41.1
    )
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(gRPC)
endif()

set(LIBS
    pthread
    grpc++
    )

target_link_libraries(server ${LIBS})
target_link_libraries(client ${LIBS})
