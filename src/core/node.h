#ifndef NODE_H
#define NODE_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <grpc++/grpc++.h>

#include "concensus_module.h"
#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::ServerAsyncResponseWriter;
using grpc::Status;

class Node {
public:
    enum class MessageID {
        RequestVote,
        AppendEntries
    };

    Node(const std::string address, const std::vector<std::string>& peer_ids);

    ~Node();

    static int current_id;

private:
    struct Tag {
        void* call;
        MessageID id;
    };

    class CallData {
    public:
        CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq);

        virtual void Proceed();
    
    protected:
        enum class CallStatus {
            Create,
            Process,
            Finish
        };
        rpc::RaftService::AsyncService* service_;
        ServerCompletionQueue* scq_;
        ServerContext ctx_;
        CallStatus status_;
    };

    class RequestVoteData : public CallData {
    public:
        RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq);

        void Proceed() override;
    
    private:
        rpc::RequestVoteRequest request_;
        rpc::RequestVoteResponse response_;
        ServerAsyncResponseWriter<rpc::RequestVoteResponse> responder_;
        Tag tag_;
    };

    class AppendEntriesData : public CallData {
    public:
        AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq);

        void Proceed() override;
    
    private:
        rpc::AppendEntriesRequest request_;
        rpc::AppendEntriesResponse response_;
        ServerAsyncResponseWriter<rpc::AppendEntriesResponse> responder_;
        Tag tag_;
    };

private:
    void HandleRPC();

private:
    int id_;
    rpc::RaftService::AsyncService service_;
    std::unique_ptr<ServerCompletionQueue> scq_;
    std::unique_ptr<Server> server_;
    Logger log_;
    std::unique_ptr<ConcensusModule> cm_;
};

}

#endif