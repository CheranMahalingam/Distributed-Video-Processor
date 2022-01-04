#include "raft_client.h"

namespace raft {

RaftClient::RaftClient(const std::string address, const std::vector<std::string>& peer_ids, CompletionQueue& cq)
    : address_(address), cq_(cq) {
    for (auto peer_id:peer_ids) {
        std::shared_ptr<Channel> chan = grpc::CreateChannel(peer_id, grpc::InsecureChannelCredentials());
        stubs_[peer_id] = rpc::RaftService::NewStub(chan);
    }
}

void RaftClient::RequestVote(const std::string peer_id, const rpc::RequestVoteRequest& request) {
    auto* call = new AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>;
    call->request = request;

    call->response_reader = stubs_[peer_id]->PrepareAsyncRequestVote(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = RaftMessageID::RequestVote;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void RaftClient::AppendEntries(const std::string peer_id, const rpc::AppendEntriesRequest& request) {
    auto* call = new AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>;
    call->request = request;

    call->response_reader = stubs_[peer_id]->PrepareAsyncAppendEntries(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = RaftMessageID::AppendEntries;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

}
