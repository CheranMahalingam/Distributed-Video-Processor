#include "client_callback_queue.h"

namespace raft {

ClientCallbackQueue::ClientCallbackQueue(const std::vector<std::string>& peer_ids, std::shared_ptr<ConcensusModule> cm, CompletionQueue& cq) 
    : peer_ids_(peer_ids), cm_(cm), cq_(cq) {}

void ClientCallbackQueue::AsyncRpcResponseHandler() {
    void* tag;
    bool ok = false;

    while (cq_.Next(&tag, &ok)) {
        GPR_ASSERT(ok);

        auto* tag_ptr = static_cast<Tag*>(tag);
        switch (tag_ptr->id) {
            case MessageID::RequestVote: {
                auto* call = static_cast<AsyncClientCall<rpc::RequestVoteResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleRequestVoteResponse(call->reply);
                } else {
                    Log(LogLevel::Error) << "RPC RequestVote call failed unexpectedly";
                }
                delete call;
                break;
            }
            case MessageID::AppendEntries: {
                auto* call = static_cast<AsyncClientCall<rpc::AppendEntriesResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleAppendEntriesResponse(call->reply);
                } else {
                    Log(LogLevel::Error) << "RPC AppendEntries call failed unexpectedly";
                }
                delete call;
                break;
            }
        }
        delete tag_ptr;
    }
}

void ClientCallbackQueue::HandleRequestVoteResponse(rpc::RequestVoteResponse reply) {
    // State changed when making calls
    if (cm_->state() != ConcensusModule::ElectionRole::Candidate) {
        Log(LogLevel::Info) << "Changed state while waiting for reply";
        return;
    }
    Log(LogLevel::Info) << "Received RequestVote reply";

    // Another server became the leader
    if (reply.term() > cm_->current_term()) {
        Log(LogLevel::Info) << "Term out of date, changed from" << cm_->current_term() << "to" << reply.term();
        cm_->ResetToFollower(reply.term());
        return;
    } else if (reply.term() == cm_->current_term()) {
        if (reply.votegranted()) {
            cm_->set_votes_received(cm_->votes_received() + 1);

            if (cm_->votes_received()*2 > peer_ids_.size()) {
                Log(LogLevel::Info) << "Wins election with" << cm_->votes_received() << "votes";
                cm_->PromoteToLeader();
                return;
            }
        }
    }
}

void ClientCallbackQueue::HandleAppendEntriesResponse(rpc::AppendEntriesResponse reply) {
    if (!reply.success()) {
        return;
    }
    Log(LogLevel::Info) << "Received AppendEntries reply";

    if (reply.term() > cm_->current_term()) {
        Log(LogLevel::Info) << "Term out of date in heartbeat reply, changed from" << cm_->current_term() << "to" << reply.term();
        cm_->ResetToFollower(reply.term());
        return;
    }
}

}
