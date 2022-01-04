#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include <memory>
#include <vector>
#include <grpc++/grpc++.h>

#include "concensus_module.h"
#include "chunk_manager.h"
#include "log.h"
#include "grpc_msg_defs.h"
#include "file_system.grpc.pb.h"

namespace file_system {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class ChunkServer {
public:
    ChunkServer(
        const std::string address,
        std::shared_ptr<raft::ConcensusModule> cm,
        std::shared_ptr<ChunkManager> manager);

    ~ChunkServer();

    void HandleRPC();

private:
    struct Tag {
        void* call;
        VideoMessageID id;
    };

    class CallData {
    public:
        CallData(
            storage::VideoService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        virtual void Proceed() = 0;

    protected:
        enum class CallStatus {
            Create,
            Process,
            Finish
        };
        storage::VideoService::AsyncService* service_;
        ServerCompletionQueue* scq_;
        raft::ConcensusModule* cm_;
        ChunkManager* manager_;
        ServerContext ctx_;
        CallStatus status_;
    };

    class UploadData : public CallData {
    public:
        UploadData(
            storage::VideoService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::UploadVideoRequest request_;
        storage::UploadVideoResponse response_;
        ServerAsyncResponseWriter<storage::UploadVideoResponse> responder_;
        Tag tag_;
    };

    class DownloadData : public CallData {
    public:
        DownloadData(
            storage::VideoService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::DownloadVideoRequest request_;
        storage::DownloadVideoResponse response_;
        ServerAsyncResponseWriter<storage::DownloadVideoResponse> responder_;
        Tag tag_;
    };

    class DeleteData : public CallData {
    public:
        DeleteData(
            storage::VideoService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::DeleteVideoRequest request_;
        storage::DeleteVideoResponse response_;
        ServerAsyncResponseWriter<storage::DeleteVideoResponse> responder_;
        Tag tag_;
    };

private:
    storage::VideoService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> scq_;
    std::unique_ptr<Server> server_;
    std::shared_ptr<raft::ConcensusModule> cm_;
    std::shared_ptr<ChunkManager> manager_;
};

}

#endif
