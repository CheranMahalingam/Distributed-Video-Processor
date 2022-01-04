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
            case RaftMessageID::RequestVote: {
                auto* call = static_cast<AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleRequestVoteResponse(call);
                } else {
                    logger(LogLevel::Error) << "RPC RequestVote call failed unexpectedly";
                }
                delete call;
                break;
            }
            case RaftMessageID::AppendEntries: {
                auto* call = static_cast<AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>*>(tag_ptr->call);
                if (call->status.ok()) {
                    HandleAppendEntriesResponse(call);
                } else {
                    logger(LogLevel::Error) << "RPC AppendEntries call failed unexpectedly";
                }
                delete call;
                break;
            }
        }
        delete tag_ptr;
    }
}

void ClientCallbackQueue::HandleRequestVoteResponse(AsyncClientCall<rpc::RequestVoteRequest, rpc::RequestVoteResponse>* call) {
    logger(LogLevel::Debug) << "Received RequestVote reply";

    // State changed when making calls
    if (cm_->state() != ConcensusModule::ElectionRole::Candidate) {
        logger(LogLevel::Debug) << "Changed state while waiting for reply";
        return;
    }

    // The concensus module term is out of date
    if (call->reply.term() > call->request.term()) {
        logger(LogLevel::Debug) << "Term out of date, changed from" << call->request.term() << "to" << call->reply.term();
        cm_->ResetToFollower(call->reply.term());
        return;
    } else if (call->reply.term() == call->request.term()) {
        if (call->reply.votegranted()) {
            cm_->set_votes_received(cm_->votes_received() + 1);

            // If the node gets votes from the majority of nodes, it becomes the leader
            if (cm_->votes_received()*2 > peer_ids_.size() + 1) {
                logger(LogLevel::Debug) << "Wins election with" << cm_->votes_received() << "votes";
                cm_->PromoteToLeader();
                return;
            }
        }
    }
}

void ClientCallbackQueue::HandleAppendEntriesResponse(AsyncClientCall<rpc::AppendEntriesRequest, rpc::AppendEntriesResponse>* call) {
    logger(LogLevel::Debug) << "Received AppendEntries reply";

    // The concensus module term is out of date
    if (call->reply.term() > call->request.term()) {
        logger(LogLevel::Debug) << "Term out of date in heartbeat reply, changed from" << call->request.term() << "to" << call->reply.term();
        cm_->ResetToFollower(call->reply.term());
        return;
    }

    if (cm_->state() == ConcensusModule::ElectionRole::Leader && call->reply.term() == cm_->current_term()) {
        // Remove "ipv4:" prefix
        std::string address = call->ctx.peer().substr(5);
        int next = cm_->log_->next_index(address);
        if (call->reply.success()) {
            cm_->log_->set_next_index(address, next + call->request.entries().size());
            cm_->log_->set_match_index(address, next + call->request.entries().size() - 1);
            logger(LogLevel::Debug) << "AppendEntries reply from" << address << "successful: next_index =" << cm_->log_->next_index(address) 
                << "match_index =" << cm_->log_->match_index(address);

            int saved_commit_index = cm_->log_->commit_index();
            int log_size = cm_->log_->entries().size();
            std::vector<rpc::LogEntry> entries(cm_->log_->entries());
            for (int i = saved_commit_index + 1; i < log_size; i++) {
                if (entries[i].term() == cm_->current_term()) {
                    int match_count = 1;
                    for (auto peer_id:peer_ids_) {
                        // Check if other nodes have this entry in their log
                        if (cm_->log_->match_index(peer_id) >= i) {
                            match_count++;
                        }
                    }

                    // If the majority of nodes have this entry in their logs, apply the commit
                    if (match_count*2 > peer_ids_.size() + 1) {
                        cm_->log_->set_commit_index(i);
                    }
                }
            }

            int new_commit_index = cm_->log_->commit_index();
            // If the commit index changed, commit entries and run the associated command
            if (new_commit_index != saved_commit_index) {
                logger(LogLevel::Debug) << "Leader sets commit_index =" << new_commit_index;

                while (cm_->log_->last_applied() < new_commit_index) {
                    cm_->log_->increment_last_applied();

                    int last_applied = cm_->log_->last_applied();
                    rpc::LogEntry uncommitted_entry = cm_->log_->entries()[last_applied];
                    cm_->CommitEntry(uncommitted_entry);
                }
            }
        } else {
            // Continue sending RPC with lower log index until the terms match
            cm_->log_->set_next_index(address, next - 1);
            logger(LogLevel::Debug) << "AppendEntries reply from" << address << "unsuccessful: next_index =" << next;
        }
    }
}

}
