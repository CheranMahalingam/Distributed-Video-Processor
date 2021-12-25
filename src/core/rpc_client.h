#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <grpc++/grpc++.h>
#include <vector>
#include <memory>

#include "log.h"
#include "raft_msg_defs.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::Channel;
using grpc::CompletionQueue;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::Status;

class RpcClient {
public:
    RpcClient(const std::string address, const std::vector<std::string>& peer_ids, CompletionQueue& cq);

    void RequestVote(const std::string peer_id, const int term);

    void AppendEntries(const std::string peer_id, const int term);

private:
    struct Tag {
        void* call;
        MessageID id;
    };

    template <class ResponseType>
    struct AsyncClientCall {
        ResponseType reply;
        ClientContext ctx;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<ResponseType>> response_reader;
    };

    std::string address_;
    std::unordered_map<std::string, std::unique_ptr<rpc::RaftService::Stub>> stubs_;
    CompletionQueue& cq_;
};

}

#endif
