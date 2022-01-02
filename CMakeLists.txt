cmake_minimum_required(VERSION 3.16)

project(VideoProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

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

option(USE_SYSTEM_PROTOBUF "Use system installed protobuf" ON)
if(USE_SYSTEM_PROTOBUF)
    find_package(Protobuf REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf
        GIT_TAG        v3.6.1
    )
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(protobuf)
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

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libavutil)

message(STATUS "Using boost ${Boost_VERSION}")
message(STATUS "Using protobuf ${Protobuf_VERSION}")
message(STATUS "Using gRPC ${gRPC_VERSION}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/protogen")

add_subdirectory(src)
