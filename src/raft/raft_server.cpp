#include "raft_server.h"

namespace raft {

RaftServer::RaftServer(
    const std::string address, 
    const std::vector<std::string>& peer_ids, 
    std::shared_ptr<ConcensusModule> cm)
    : cm_(cm) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
}

RaftServer::~RaftServer() {
    server_->Shutdown();
    scq_->Shutdown();
}

void RaftServer::HandleRPC() {
    new RequestVoteData{&service_, scq_.get(), cm_.get()};
    new AppendEntriesData{&service_, scq_.get(), cm_.get()};
    void* tag;
    bool ok;
    while (true) {
        if (scq_->Next(&tag, &ok) && ok) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            switch (tag_ptr->id) {
                case RaftMessageID::RequestVote: {
                    static_cast<RequestVoteData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case RaftMessageID::AppendEntries: {
                    static_cast<AppendEntriesData*>(tag_ptr->call)->Proceed();
                    break;
                }
            }
        } else {
            logger(LogLevel::Error) << "RPC call failed";
        }
    }
}

RaftServer::CallData::CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm)
    : service_(service), scq_(scq), cm_(cm), status_(CallStatus::Create) {
}

RaftServer::RequestVoteData::RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = RaftMessageID::RequestVote;
    tag_.call = this;
    Proceed();
}

void RaftServer::RequestVoteData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            logger(LogLevel::Debug) << "Creating RequestVote reply...";
            status_ = CallStatus::Process;
            service_->RequestRequestVote(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            logger(LogLevel::Debug) << "Processing RequestVote reply...";
            new RequestVoteData{service_, scq_, cm_};

            // If the node is dead responds with failure
            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            int last_log_index = cm_->log_->LastLogIndex();
            int last_log_term = cm_->log_->LastLogTerm();

            // If the concensus module term is out of date the term and state are reset
            if (request_.term() > cm_->current_term()) {
                logger(LogLevel::Debug) << "Term out of date in RequestVote RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            // Ensure that the node has not voted for a different node
            // Ensure that the request log term is not out of date
            // request log terms: 1, 2, 2, 3   node log terms: 1, 2, 2, 3, 4     INVALID
            // request log terms: 1, 2, 2      node log terms: 1, 2, 2, 2        INVALID
            // request log terms: 1, 2, 3, 3   node log terms: 1, 2, 3           VALID
            if (request_.term() == cm_->current_term() &&
               (cm_->vote() == "" || cm_->vote() == request_.candidateid()) &&
               (request_.lastlogterm() > last_log_term ||
               (request_.lastlogterm() == last_log_term && request_.lastlogindex() >= last_log_index))) {
                response_.set_votegranted(true);
                cm_->set_vote(request_.candidateid());

                logger(LogLevel::Debug) << "Resetting election timer...";
                cm_->ElectionTimeout(request_.term());
            } else {
                response_.set_votegranted(false);
            }

            response_.set_term(cm_->current_term());
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

RaftServer::AppendEntriesData::AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = RaftMessageID::AppendEntries;
    tag_.call = this;
    Proceed();
}

void RaftServer::AppendEntriesData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            logger(LogLevel::Debug) << "Creating AppendEntries reply...";
            status_ = CallStatus::Process;
            service_->RequestAppendEntries(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            logger(LogLevel::Debug) << "Processing AppendEntries reply...";
            new AppendEntriesData{service_, scq_, cm_};

            // If the node is dead responds with failure
            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            // If the concensus module term is out of date the term and state are reset
            if (request_.term() > cm_->current_term()) {
                logger(LogLevel::Debug) << "Term out of date in AppendEntries RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            bool success = false;
            if (request_.term() == cm_->current_term()) {
                // Acknowledging the Heartbeat means the server knows a healthy leader exists so the election timer is reset
                if (cm_->state() != ConcensusModule::ElectionRole::Follower) {
                    cm_->ResetToFollower(request_.term());
                } else {
                    logger(LogLevel::Debug) << "Resetting Election Timer...";
                    cm_->ElectionTimeout(request_.term());
                }

                // Attempt to update log if term is consistent between leader and follower at log index
                if (request_.prevlogindex() == -1 ||
                    (request_.prevlogindex() < cm_->log_->entries().size() && request_.prevlogterm() == cm_->log_->entries()[request_.prevlogindex()].term())) {
                    success = true;

                    int log_insert_index = request_.prevlogindex() + 1;
                    int new_entries_index = 0;

                    // Search for a point where there is a mismatch of terms between the existing logs and the new entries
                    while (log_insert_index < cm_->log_->entries().size() && 
                        new_entries_index < request_.entries().size() && 
                        cm_->log_->entries()[log_insert_index].term() == request_.entries()[new_entries_index].term()) {
                        log_insert_index++;
                        new_entries_index++;
                    }

                    // Update log with new entries from leader
                    if (new_entries_index < request_.entries().size()) {
                        std::vector<rpc::LogEntry> new_entries(request_.entries().begin() + new_entries_index, request_.entries().end());
                        cm_->log_->InsertLog(log_insert_index, new_entries);
                        cm_->PersistLogToStorage(cm_->log_->entries(), false);
                    }

                    // If the commit index is behind, apply the entries committed by the leader
                    if ((int)request_.leadercommit() > cm_->log_->commit_index()) {
                        int new_commit_index = std::min((std::size_t)request_.leadercommit(), cm_->log_->entries().size());
                        cm_->log_->set_commit_index(new_commit_index);
                        logger(LogLevel::Debug) << "Setting commit index =" << new_commit_index;

                        while (cm_->log_->last_applied() < new_commit_index) {
                            cm_->log_->increment_last_applied();

                            int last_applied = cm_->log_->last_applied();
                            rpc::LogEntry uncommitted_entry = cm_->log_->entries()[last_applied];
                            cm_->CommitEntry(uncommitted_entry);
                        }
                    }
                }
            }

            response_.set_term(cm_->current_term());
            response_.set_success(success);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

}
