#include "rpc_client.h"

namespace raft {

RpcClient::RpcClient(const std::string address, const std::vector<std::string>& peer_ids, CompletionQueue& cq)
    : address_(address), cq_(cq) {
    for (auto peer_id:peer_ids) {
        std::shared_ptr<Channel> chan = grpc::CreateChannel(peer_id, grpc::InsecureChannelCredentials());
        stubs_[peer_id] = rpc::RaftService::NewStub(chan);
    }
}

void RpcClient::RequestVote(const std::string peer_id, const int term) {
    rpc::RequestVoteRequest request;
    request.set_term(term);
    request.set_candidateid(address_);
    // TODO: Update log index + term
    request.set_lastlogindex(0);
    request.set_lastlogterm(0);

    auto* call = new AsyncClientCall<rpc::RequestVoteResponse>;

    call->response_reader = stubs_[peer_id]->PrepareAsyncRequestVote(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = MessageID::RequestVote;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void RpcClient::AppendEntries(const std::string peer_id, const int term) {
    rpc::AppendEntriesRequest request;
    request.set_term(term);
    request.set_leaderid(address_);
    // TODO: Hydrate rpc call fields with correct values
    request.set_prevlogindex(0);
    request.set_prevlogterm(0);
    // request.set_entries("0");
    request.set_leadercommit(0);

    auto* call = new AsyncClientCall<rpc::AppendEntriesResponse>;

    call->response_reader = stubs_[peer_id]->PrepareAsyncAppendEntries(&call->ctx, request, &cq_);

    call->response_reader->StartCall();

    auto* tag = new Tag;
    tag->call = (void*)call;
    tag->id = MessageID::AppendEntries;
    call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

}
