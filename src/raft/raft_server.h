#ifndef RAFT_SERVER_H
#define RAFT_SERVER_H

#include <vector>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>
#include <algorithm>

#include "concensus_module.h"
#include "log.h"
#include "grpc_msg_defs.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class RaftServer {
public:
    RaftServer(
        const std::string address, 
        const std::vector<std::string>& peer_ids, 
        std::shared_ptr<ConcensusModule> cm);

    ~RaftServer();

    void HandleRPC();

private:
    struct Tag {
        void* call;
        RaftMessageID id;
    };

    class CallData {
    public:
        CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm);

        virtual void Proceed() = 0;

    protected:
        enum class CallStatus {
            Create,
            Process,
            Finish
        };
        rpc::RaftService::AsyncService* service_;
        ServerCompletionQueue* scq_;
        ConcensusModule* cm_;
        ServerContext ctx_;
        CallStatus status_;
    };

    class RequestVoteData : public CallData {
    public:
        RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm);

        void Proceed() override;

    private:
        rpc::RequestVoteRequest request_;
        rpc::RequestVoteResponse response_;
        ServerAsyncResponseWriter<rpc::RequestVoteResponse> responder_;
        Tag tag_;
    };

    class AppendEntriesData : public CallData {
    public:
        AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm);

        void Proceed() override;

    private:
        rpc::AppendEntriesRequest request_;
        rpc::AppendEntriesResponse response_;
        ServerAsyncResponseWriter<rpc::AppendEntriesResponse> responder_;
        Tag tag_;
    };

private:
    rpc::RaftService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> scq_;
    std::unique_ptr<Server> server_;
    std::shared_ptr<ConcensusModule> cm_;
};

}

#endif
