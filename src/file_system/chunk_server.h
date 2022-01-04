#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include <grpc++/grpc++.h>

#include "async_server.h"
#include "concensus_module.h"
#include "chunk_manager.h"
#include "log.h"
#include "grpc_msg_defs.h"
#include "file_system.grpc.pb.h"
#include "server.grpc.pb.h"

namespace file_system {

using grpc::ServerCompletionQueue;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class ChunkServer : public AsyncServer {
public:
    ChunkServer();

    ~ChunkServer();

    class UploadData : public CallData {
    public:
        UploadData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::UploadVideoRequest request_;
        storage::UploadVideoResponse response_;
        ServerAsyncResponseWriter<storage::UploadVideoResponse> responder_;
        ChunkManager* manager_;
        Tag tag_;
    };

    class DownloadData : public CallData {
    public:
        DownloadData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::DownloadVideoRequest request_;
        storage::DownloadVideoResponse response_;
        ServerAsyncResponseWriter<storage::DownloadVideoResponse> responder_;
        ChunkManager* manager_;
        Tag tag_;
    };

    class DeleteData : public CallData {
    public:
        DeleteData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            raft::ConcensusModule* cm,
            ChunkManager* manager);

        void Proceed() override;

    private:
        storage::DeleteVideoRequest request_;
        storage::DeleteVideoResponse response_;
        ServerAsyncResponseWriter<storage::DeleteVideoResponse> responder_;
        ChunkManager* manager_;
        Tag tag_;
    };
};

}

#endif
