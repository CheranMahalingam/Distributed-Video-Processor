#include "rpc_server.h"

namespace raft {

RpcServer::RpcServer(
    boost::asio::io_context& io_context, 
    const std::string address, 
    const std::vector<std::string>& peer_ids, 
    std::shared_ptr<ConcensusModule> cm)
    : io_(io_context), cm_(cm) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
}

RpcServer::~RpcServer() {
    server_->Shutdown();
    scq_->Shutdown();
}

void RpcServer::HandleRPC() {
    new RequestVoteData{&service_, scq_.get(), cm_.get()};
    new AppendEntriesData{&service_, scq_.get(), cm_.get()};
    void* tag;
    bool ok;
    while (true) {
        if (scq_->Next(&tag, &ok) && ok) {
            auto* tag_ptr = static_cast<Tag*>(tag);
            switch (tag_ptr->id) {
                case MessageID::RequestVote: {
                    static_cast<RequestVoteData*>(tag_ptr->call)->Proceed();
                    break;
                }
                case MessageID::AppendEntries: {
                    static_cast<AppendEntriesData*>(tag_ptr->call)->Proceed();
                    break;
                }
            }
        } else {
            logger(LogLevel::Error) << "RPC call failed";
        }
    }
}

RpcServer::CallData::CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm)
    : service_(service), scq_(scq), cm_(cm), status_(CallStatus::Create) {
}

RpcServer::RequestVoteData::RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::RequestVote;
    tag_.call = this;
    Proceed();
}

void RpcServer::RequestVoteData::Proceed() {
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

            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            int last_log_index = cm_->log().LastLogIndex();
            int last_log_term = cm_->log().LastLogTerm();

            if (request_.term() > cm_->current_term()) {
                logger(LogLevel::Debug) << "Term out of date in RequestVote RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            // Election safety to prevent node with out of date logs to be elected
            if (request_.term() == cm_->current_term() && 
               (cm_->vote() == "" || cm_->vote() == request_.candidateid()) &&
               (request_.lastlogterm() > last_log_term ||
               (request_.lastlogterm() == last_log_term && request_.lastlogindex() >= last_log_index))) {
                response_.set_votegranted(true);
                cm_->set_vote(request_.candidateid());
                logger(LogLevel::Debug) << "Reset...";
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

RpcServer::AppendEntriesData::AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::AppendEntries;
    tag_.call = this;
    Proceed();
}

void RpcServer::AppendEntriesData::Proceed() {
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

            if (cm_->state() == ConcensusModule::ElectionRole::Dead) {
                status_ = CallStatus::Finish;
                responder_.Finish(response_, Status::CANCELLED, (void*)&tag_);
                break;
            }

            if (request_.term() > cm_->current_term()) {
                logger(LogLevel::Debug) << "Term out of date in AppendEntries RPC, changed from" << cm_->current_term() << "to" << request_.term();
                cm_->ResetToFollower(request_.term());
            }

            bool success = false;
            if (request_.term() == cm_->current_term()) {
                if (cm_->state() != ConcensusModule::ElectionRole::Follower) {
                    cm_->ResetToFollower(request_.term());
                } else {
                    logger(LogLevel::Debug) << "Resetting Election Timer...";
                    cm_->ElectionTimeout(request_.term());
                }

                // Attempt to update log if term is consistent between leader and follower at log index
                if (request_.prevlogindex() == -1 ||
                    (request_.prevlogindex() < cm_->log().entries().size() && request_.prevlogterm() == cm_->log().entries()[request_.prevlogindex()].term())) {
                    success = true;

                    int log_insert_index = request_.prevlogindex() + 1;
                    int new_entries_index = 0;

                    // Search for a point where there is a mismatch of terms between the existing logs and the new entries
                    while (log_insert_index < cm_->log().entries().size() && 
                        new_entries_index < request_.entries().size() && 
                        cm_->log().entries()[log_insert_index].term() == request_.entries()[new_entries_index].term()) {
                        log_insert_index++;
                        new_entries_index++;
                    }

                    // Update followers log with new entries
                    if (new_entries_index < request_.entries().size()) {
                        std::vector<rpc::LogEntry> new_entries(request_.entries().begin() + new_entries_index, request_.entries().end());
                        cm_->log().InsertLog(log_insert_index, new_entries);
                        cm_->PersistLogToStorage(cm_->log().entries(), false);
                    }

                    if ((int)request_.leadercommit() > cm_->log().commit_index()) {
                        int new_commit_index = std::min((std::size_t)request_.leadercommit(), cm_->log().entries().size());
                        cm_->log().set_commit_index(new_commit_index);
                        logger(LogLevel::Debug) << "Setting commit index =" << new_commit_index;

                        while (cm_->log().last_applied() < new_commit_index) {
                            cm_->log().increment_last_applied();

                            int last_applied = cm_->log().last_applied();
                            rpc::LogEntry uncommitted_entry = cm_->log().entries()[last_applied];
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
