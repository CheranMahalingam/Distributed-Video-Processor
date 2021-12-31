get_filename_component(raft_proto "../protos/raft.proto" ABSOLUTE)
get_filename_component(raft_proto_path "${raft_proto}" PATH)

set(raft_proto_srcs "${PROJECT_SOURCE_DIR}/protogen/raft.pb.cc")
set(raft_proto_hdrs "${PROJECT_SOURCE_DIR}/protogen/raft.pb.h")
set(raft_grpc_srcs "${PROJECT_SOURCE_DIR}/protogen/raft.grpc.pb.cc")
set(raft_grpc_hdrs "${PROJECT_SOURCE_DIR}/protogen/raft.grpc.pb.h")
add_custom_command(
    OUTPUT "${raft_proto_srcs}" "${raft_proto_hdrs}" "${raft_grpc_srcs}" "${raft_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${PROJECT_SOURCE_DIR}/protogen"
        --cpp_out "${PROJECT_SOURCE_DIR}/protogen"
        -I "${raft_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${raft_proto}"
    DEPENDS "${raft_proto}")

add_library(raft_grpc_proto
    ${raft_grpc_srcs}
    ${raft_grpc_hdrs}
    ${raft_proto_srcs}
    ${raft_proto_hdrs})
target_link_libraries(raft_grpc_proto
    grpc++_reflection
    grpc++
    libprotoc
    libprotobuf)

add_executable(VideoProcessor 
    raft/rpc_server.cpp
    raft/rpc_client.cpp 
    raft/client_callback_queue.cpp 
    raft/concensus_module.cpp
    raft/command_log.cpp
    raft/snapshot.cpp
    raft/commit_channel.cpp
    common/log.cpp
    main.cpp)
target_link_libraries(VideoProcessor
    raft_grpc_proto)
target_include_directories(VideoProcessor
    PRIVATE
    common
    raft
    "${PROJECT_SOURCE_DIR}/protogen")
