#ifndef CLIENT_CALLBACK_QUEUE_H
#define CLIENT_CALLBACK_QUEUE_H

#include <grpc++/grpc++.h>
#include <memory>
#include <vector>
#include <string>

#include "concensus_module.h"
#include "log.h"
#include "raft_msg_defs.h"
#include "raft.grpc.pb.h"

namespace raft {

using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::Status;

class ClientCallbackQueue {
public:
    template <class RequestType, class ResponseType>
    struct AsyncClientCall {
        RequestType request;
        ResponseType reply;
        ClientContext ctx;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<ResponseType>> response_reader;
    };

    ClientCallbackQueue(const std::vector<std::string>& peer_ids, std::shared_ptr<ConcensusModule> cm, CompletionQueue& cq);

    void AsyncRpcResponseHandler();

private:
    void HandleRequestVoteResponse(AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>* call);

    void HandleAppendEntriesResponse(AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>* call);

private:
    struct Tag {
        void* call;
        MessageID id;
    };

    std::vector<std::string> peer_ids_;
    std::shared_ptr<ConcensusModule> cm_;
    CompletionQueue& cq_;
};

}

#endif