#ifndef RAFT_SERVER_H
#define RAFT_SERVER_H

#include <vector>
#include <grpc++/grpc++.h>

#include "async_server.h"
#include "concensus_module.h"
#include "log.h"
#include "grpc_msg_defs.h"
#include "raft.grpc.pb.h"
#include "server.grpc.pb.h"

namespace raft {

using grpc::ServerCompletionQueue;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class RaftServer : public AsyncServer {
public:
    RaftServer();

    ~RaftServer();

    class RequestVoteData : public CallData {
    public:
        RequestVoteData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            ConcensusModule* cm);

        void Proceed() override;

    private:
        rpc::RequestVoteRequest request_;
        rpc::RequestVoteResponse response_;
        ServerAsyncResponseWriter<rpc::RequestVoteResponse> responder_;
        Tag tag_;
    };

    class AppendEntriesData : public CallData {
    public:
        AppendEntriesData(
            server::VideoProcessorService::AsyncService* service,
            ServerCompletionQueue* scq,
            ConcensusModule* cm);

        void Proceed() override;

    private:
        rpc::AppendEntriesRequest request_;
        rpc::AppendEntriesResponse response_;
        ServerAsyncResponseWriter<rpc::AppendEntriesResponse> responder_;
        Tag tag_;
    };
};

}

#endif
