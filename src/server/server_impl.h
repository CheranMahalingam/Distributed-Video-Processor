#ifndef SERVER_IMPL_H
#define SERVER_IMPL_H

#include <string>
#include <memory>
#include <grpc++/grpc++.h>

#include "grpc_msg_defs.h"
#include "concensus_module.h"
#include "chunk_manager.h"
#include "chunk_server.h"
#include "raft_server.h"
#include "server.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class ServerImpl {
public:
    ServerImpl(
        const std::string address,
        std::shared_ptr<raft::ConcensusModule> cm,
        std::shared_ptr<file_system::ChunkManager> manager);

    ~ServerImpl();

    void HandleRPC();

private:
    struct Tag {
        void* call;
        RpcCommandID id;
    };
    server::VideoProcessorService::AsyncService service_;
    std::unique_ptr<raft::RaftServer> raft_server_;
    std::unique_ptr<file_system::ChunkServer> chunk_server_;
    std::shared_ptr<raft::ConcensusModule> cm_;
    std::shared_ptr<file_system::ChunkManager> manager_;
    std::unique_ptr<Server> server_;
    std::unique_ptr<ServerCompletionQueue> scq_;
};

#endif
