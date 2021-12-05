cmake_minimum_required(VERSION 3.16)

project(DistributedVideoProcessor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

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

set(_REFLECTION grpc++_reflection)
set(_GRPC_GRPCPP grpc++)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_PROTOBUF_LIBPROTOBUF $<TARGET_FILE:protoc>)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

get_filename_component(video_proto "protos/video.proto" ABSOLUTE)
get_filename_component(video_proto_path "${video_proto}" PATH)

set(video_proto_srcs "${PROJECT_SOURCE_DIR}/protogen/video.pb.cc")
set(video_proto_hdrs "${PROJECT_SOURCE_DIR}/protogen/video.pb.h")
set(video_grpc_srcs "${PROJECT_SOURCE_DIR}/protogen/video.grpc.pb.cc")
set(video_grpc_hdrs "${PROJECT_SOURCE_DIR}/protogen/video.grpc.pb.h")
add_custom_command(
    OUTPUT "${video_proto_srcs}" "${video_proto_hdrs}" "${video_grpc_srcs}" "${video_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${PROJECT_SOURCE_DIR}/protogen"
        --cpp_out "${PROJECT_SOURCE_DIR}/protogen"
        -I "${video_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${video_proto}"
    DEPENDS "${video_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(video_grpc_proto
    ${video_grpc_srcs}
    ${video_grpc_hdrs}
    ${video_proto_srcs}
    ${video_proto_hdrs})
target_link_libraries(video_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

foreach(_target
    async_server async_client)
    add_executable(${_target} "src/${_target}.cpp")
    target_link_libraries(${_target}
        video_grpc_proto
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF})
endforeach()
