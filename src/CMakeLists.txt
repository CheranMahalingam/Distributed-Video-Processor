foreach(target raft file_system)
    get_filename_component(proto "../protos/${target}.proto" ABSOLUTE)
    get_filename_component(proto_path "${proto}" PATH)
    set(proto_srcs "${PROJECT_SOURCE_DIR}/protogen/${target}.pb.cc")
    set(proto_hdrs "${PROJECT_SOURCE_DIR}/protogen/${target}.pb.h")
    set(grpc_srcs "${PROJECT_SOURCE_DIR}/protogen/${target}.grpc.pb.cc")
    set(grpc_hdrs "${PROJECT_SOURCE_DIR}/protogen/${target}.grpc.pb.h")

    add_custom_command(
        OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${PROJECT_SOURCE_DIR}/protogen"
            --cpp_out "${PROJECT_SOURCE_DIR}/protogen"
            -I "${proto_path}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${proto}"
        DEPENDS "${proto}")

    add_library("grpc_${target}_proto"
        ${grpc_srcs}
        ${grpc_hdrs}
        ${proto_srcs}
        ${proto_hdrs})

    target_link_libraries("grpc_${target}_proto"
        grpc++_reflection
        grpc++
        libprotoc
        libprotobuf)
endforeach()

add_executable(VideoProcessor
    common/log.cpp
    raft/raft_server.cpp
    raft/raft_client.cpp 
    raft/client_callback_queue.cpp 
    raft/concensus_module.cpp
    raft/command_log.cpp
    raft/snapshot.cpp
    raft/commit_channel.cpp
    file_system/chunk_manager.cpp
    file_system/chunk_server.cpp
    main.cpp)
target_link_libraries(VideoProcessor
    grpc_raft_proto
    grpc_file_system_proto
    PkgConfig::LIBAV)
target_include_directories(VideoProcessor
    PRIVATE
    common
    raft
    file_system
    processor
    "${PROJECT_SOURCE_DIR}/protogen")
