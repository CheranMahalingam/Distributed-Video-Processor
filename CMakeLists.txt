cmake_minimum_required(VERSION 3.16)

project(VideoProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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
    find_package(Protobuf 3.6 REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf
        GIT_TAG        v3.19.1
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

set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/protogen")

set(FILENAME "raft")
get_filename_component(raft_proto "protos/${FILENAME}.proto" ABSOLUTE)
get_filename_component(raft_proto_path "${raft_proto}" PATH)

set(proto_srcs "${PROJECT_SOURCE_DIR}/protogen/${FILENAME}.pb.cc")
set(proto_hdrs "${PROJECT_SOURCE_DIR}/protogen/${FILENAME}.pb.h")
set(grpc_srcs "${PROJECT_SOURCE_DIR}/protogen/${FILENAME}.grpc.pb.cc")
set(grpc_hdrs "${PROJECT_SOURCE_DIR}/protogen/${FILENAME}.grpc.pb.h")
add_custom_command(
    OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${PROJECT_SOURCE_DIR}/protogen"
        --cpp_out "${PROJECT_SOURCE_DIR}/protogen"
        -I "${raft_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${raft_proto}"
    DEPENDS "${raft_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(grpc_proto
    ${grpc_srcs}
    ${grpc_hdrs}
    ${proto_srcs}
    ${proto_hdrs})
target_link_libraries(grpc_proto
    grpc++_reflection
    grpc++
    libprotoc
    libprotobuf)

add_executable(VideoProcessor 
    src/core/rpc_server.cpp
    src/core/rpc_client.cpp 
    src/core/client_callback_queue.cpp 
    src/core/concensus_module.cpp 
    src/common/log.cpp
    src/main.cpp)
target_link_libraries(VideoProcessor
    grpc_proto)
target_include_directories(VideoProcessor
    PRIVATE
    "${PROJECT_SOURCE_DIR}/src/common"
    "${PROJECT_SOURCE_DIR}/src/core"
    "${PROJECT_SOURCE_DIR}/protogen")
