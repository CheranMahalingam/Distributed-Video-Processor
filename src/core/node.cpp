#include "node.h"

namespace raft {

Node::Node(const std::string address, const std::vector<std::string>& peer_ids)
    : id_(current_id), log_(Logger()) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    scq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();

    boost::asio::io_context io_context;
    cm_ = std::make_unique<ConcensusModule>(id_, io_context, peer_ids, log_);
    current_id++;
}

Node::~Node() {
    server_->Shutdown();
    scq_->Shutdown();
}

void Node::HandleRPC() {
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
            log_ << "RPC call failed";
        }
    }
}

Node::CallData::CallData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm)
    : service_(service), scq_(scq), cm_(cm) {
    Proceed();
}

Node::RequestVoteData::RequestVoteData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::RequestVote;
    tag_.call = this;
    Proceed();
}

void Node::RequestVoteData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            status_ = CallStatus::Process;
            service_->RequestRequestVote(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            new RequestVoteData{service_, scq_, cm_};
            response_.set_term(cm_->current_term());
            response_.set_votegranted(true);
            status_ = CallStatus::Finish;
            responder_.Finish(response_, Status::OK, (void*)&tag_);
            break;
        }
        default: {
            delete this;
        }
    }
}

Node::AppendEntriesData::AppendEntriesData(rpc::RaftService::AsyncService* service, ServerCompletionQueue* scq, ConcensusModule* cm) 
    : CallData{service, scq, cm}, responder_(&ctx_) {
    tag_.id = MessageID::AppendEntries;
    tag_.call = this;
    Proceed();
}

void Node::AppendEntriesData::Proceed() {
    switch (status_) {
        case CallStatus::Create: {
            status_ = CallStatus::Process;
            service_->RequestAppendEntries(&ctx_, &request_, &responder_, scq_, scq_, (void*)&tag_);
            break;
        }
        case CallStatus::Process: {
            new AppendEntriesData{service_, scq_, cm_};
            response_.set_term(cm_->current_term());
            response_.set_success(true);
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
