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
                auto* call = static_cast<AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleRequestVoteResponse(call);
                } else {
                    Log(LogLevel::Error) << "RPC RequestVote call failed unexpectedly";
                }
                delete call;
                break;
            }
            case MessageID::AppendEntries: {
                auto* call = static_cast<AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleAppendEntriesResponse(call);
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

void ClientCallbackQueue::HandleRequestVoteResponse(AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>* call) {
    Log(LogLevel::Info) << "Received RequestVote reply";

    // State changed when making calls
    if (cm_->state() != ConcensusModule::ElectionRole::Candidate) {
        Log(LogLevel::Info) << "Changed state while waiting for reply";
        return;
    }

    // Another server became the leader
    if (call->reply.term() > cm_->current_term()) {
        Log(LogLevel::Info) << "Term out of date, changed from" << cm_->current_term() << "to" << call->reply.term();
        cm_->ResetToFollower(call->reply.term());
        return;
    } else if (call->reply.term() == cm_->current_term()) {
        if (call->reply.votegranted()) {
            cm_->set_votes_received(cm_->votes_received() + 1);

            if (cm_->votes_received()*2 > peer_ids_.size() + 1) {
                Log(LogLevel::Info) << "Wins election with" << cm_->votes_received() << "votes";
                cm_->PromoteToLeader();
                return;
            }
        }
    }
}

void ClientCallbackQueue::HandleAppendEntriesResponse(AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>* call) {
    if (!call->reply.success()) {
        return;
    }
    Log(LogLevel::Info) << "Received AppendEntries reply";

    if (call->reply.term() > cm_->current_term()) {
        Log(LogLevel::Info) << "Term out of date in heartbeat reply, changed from" << cm_->current_term() << "to" << call->reply.term();
        cm_->ResetToFollower(call->reply.term());
        return;
    }

    if (cm_->state() == ConcensusModule::ElectionRole::Leader && call->reply.term() == cm_->current_term()) {
        CommandLog log(cm_->log());
        int next = log.next_index(call->ctx.peer());
        if (call->reply.success()) {
            int next = log.next_index(call->ctx.peer());
            log.set_next_index(call->ctx.peer(), next + call->request.entries().size());
            log.set_match_index(call->ctx.peer(), next - 1);
            Log(LogLevel::Info) << "AppendEntries reply from" << call->ctx.peer() << "successful: next_index =" << log.next_index(call->ctx.peer()) << "match_index =" << log.match_index(call->ctx.peer());

            int saved_commit_index = log.commit_index();
            int log_size = log.entries().size();
            std::vector<rpc::LogEntry> entries(log.entries());
            for (int i = saved_commit_index + 1; i < log_size; i++) {
                if (entries[i].term() == cm_->current_term()) {
                    int match_count = 1;
                    for (auto peer_id:peer_ids_) {
                        if (log.match_index(peer_id) >= i) {
                            match_count++;
                        }
                    }

                    if (match_count*2 > peer_ids_.size() + 1) {
                        log.set_commit_index(i);
                    }
                }
            }

            if (log.commit_index() != saved_commit_index) {
                Log(LogLevel::Info) << "Leader sets commit_index =" << log.commit_index();
            }
        } else {
            log.set_next_index(call->ctx.peer(), next);
            Log(LogLevel::Info) << "AppendEntries reply from" << call->ctx.peer() << "unsuccessful: next_index =" << next;
        }
    }
}

}
