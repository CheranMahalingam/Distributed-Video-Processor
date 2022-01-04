#ifndef RAFT_CLIENT_H
#define RAFT_CLIENT_H

#include <grpc++/grpc++.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "log.h"
#include "grpc_msg_defs.h"
#include "raft.grpc.pb.h"
#include "server.grpc.pb.h"

namespace raft {

using grpc::Channel;
using grpc::CompletionQueue;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::Status;

class RaftClient {
public:
    RaftClient(const std::string address, const std::vector<std::string>& peer_ids, CompletionQueue& cq);

    void RequestVote(const std::string peer_id, const rpc::RequestVoteRequest& request);

    void AppendEntries(const std::string peer_id, const rpc::AppendEntriesRequest& request);

private:
    struct Tag {
        void* call;
        RpcCommandID id;
    };

    template <class RequestType, class ResponseType>
    struct AsyncClientCall {
        RequestType request;
        ResponseType reply;
        ClientContext ctx;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<ResponseType>> response_reader;
    };

    std::string address_;
    std::unordered_map<std::string, std::unique_ptr<server::VideoProcessorService::Stub>> stubs_;
    CompletionQueue& cq_;
};

}

#endif
